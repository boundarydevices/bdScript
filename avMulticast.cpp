/*
 * Module avSendTo.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: avSendTo.cpp,v $
 * Revision 1.21  2006-12-01 18:30:42  tkisky
 * -touchscreen device name
 *
 * Revision 1.20  2006/03/28 04:10:33  ericn
 * -bring up to date (qualify global routines)
 *
 * Revision 1.19  2003/11/24 22:49:43  ericn
 * -max volume
 *
 * Revision 1.18  2003/11/04 13:09:32  ericn
 * -added keyence support, beep
 *
 * Revision 1.17  2003/11/04 00:40:12  tkisky
 * -htons
 *
 * Revision 1.16  2003/11/02 17:59:06  ericn
 * -added symbol scanner support
 *
 * Revision 1.15  2003/10/31 13:57:57  ericn
 * -modified to ignore duplicate barcodes (within 1 second)
 *
 * Revision 1.14  2003/10/31 13:48:45  ericn
 * -removed some debug msgs
 *
 * Revision 1.13  2003/10/31 13:32:54  ericn
 * -inverted guard shack pin, added barcode type and terminator
 *
 * Revision 1.12  2003/10/20 03:49:27  ericn
 * -set params for output audio fd
 *
 * Revision 1.11  2003/10/19 17:02:56  ericn
 * -added U/I
 *
 * Revision 1.10  2003/10/18 19:15:28  ericn
 * -made udp rx handler polled
 *
 * Revision 1.9  2003/10/18 16:37:03  ericn
 * -added logo
 *
 * Revision 1.8  2003/10/17 05:59:04  ericn
 * -added doorbell
 *
 * Revision 1.7  2003/10/05 19:13:20  ericn
 * -added barcode, touch, and GPIO
 *
 * Revision 1.6  2003/10/05 04:34:06  ericn
 * -modified to force Mono output
 *
 * Revision 1.5  2003/10/04 18:31:43  ericn
 * -removed comments
 *
 * Revision 1.4  2003/10/01 05:04:24  ericn
 * -modified to remove echo
 *
 * Revision 1.3  2003/10/01 01:07:55  ericn
 * -added udpRxThread for receiving audio
 *
 * Revision 1.2  2003/09/22 02:07:35  ericn
 * -modified to keep running with no camera
 *
 * Revision 1.1  2003/09/14 18:08:03  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "avSendTo.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>
#include <sys/mman.h>
#include <time.h>
#include <poll.h>
#include "hexDump.h"
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "barcodePoll.h"
#include "tickMs.h"
#include "gpioPoll.h"
#include "fbDev.h"
#include <errno.h>
#include <stddef.h>
#include "imgJPEG.h"
#include "multicastSocket.h"

extern "C" {
#include <jpeglib.h>
};

#define BYTESPERMEMCHUNK 4096

struct memChunk_t {
   unsigned long  length_ ; // bytes used so far
   memChunk_t    *next_ ;   // next chunk
   JOCTET         data_[BYTESPERMEMCHUNK-8];
};

struct memDest_t {
   struct       jpeg_destination_mgr pub; /* public fields */
   memChunk_t  *chunkHead_ ;
   memChunk_t  *chunkTail_ ;
};

typedef memDest_t  *memDestPtr_t ;

/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */
static void init_destination (j_compress_ptr cinfo)
{
   memDestPtr_t dest = (memDestPtr_t) cinfo->dest;
   
   assert( 0 == dest->chunkHead_ );

   /* Allocate first memChunk */
   dest->chunkHead_ = dest->chunkTail_ = new memChunk_t ;
   memset( dest->chunkHead_, 0, sizeof( *dest->chunkHead_ ) );
   dest->pub.next_output_byte = dest->chunkHead_->data_ ;
   dest->pub.free_in_buffer   = sizeof( dest->chunkHead_->data_ );
}


/*
 * Empty the output buffer --- called whenever buffer fills up.
 */

static boolean empty_output_buffer (j_compress_ptr cinfo)
{
   memDestPtr_t const dest = (memDestPtr_t) cinfo->dest;
   
   assert( 0 != dest->chunkTail_ );

   //
   // free_in_buffer member doesn't seem to be filled in
   //
   dest->chunkTail_->length_ = sizeof( dest->chunkTail_->data_ ); //  - dest->pub.free_in_buffer ;

   memChunk_t * const next = new memChunk_t ;
   memset( next, 0, sizeof( *next ) );
   dest->chunkTail_->next_ = next ;
   dest->chunkTail_ = next ;
   
   dest->pub.next_output_byte = next->data_ ;
   dest->pub.free_in_buffer   = sizeof( next->data_ );

   return TRUE;
}


/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */
static void term_destination (j_compress_ptr cinfo)
{
   //
   // just account for data used
   //
   memDest_t  *const dest = (memDestPtr_t) cinfo->dest ;
   memChunk_t  *tail = dest->chunkTail_ ;
   assert( 0 != tail );   
   assert( sizeof( tail->data_ ) >= dest->pub.free_in_buffer );
   assert( tail->data_ <= dest->pub.next_output_byte );
   assert( tail->data_ + sizeof( tail->data_ ) >= dest->pub.next_output_byte + dest->pub.free_in_buffer );

   tail->length_ = sizeof( tail->data_ ) - dest->pub.free_in_buffer ;
}

/*
 * Prepare for output to a chunked memory stream.
 */
void jpeg_mem_dest( j_compress_ptr cinfo )
{
   assert( 0 == cinfo->dest );
   cinfo->dest = (struct jpeg_destination_mgr *)
                 (*cinfo->mem->alloc_small)
                     ( (j_common_ptr) cinfo, 
                       JPOOL_IMAGE,
		       sizeof(memDest_t)
                     );
   memDest_t *dest = (memDest_t *) cinfo->dest ;
   dest->pub.init_destination    = init_destination ;
   dest->pub.empty_output_buffer = empty_output_buffer ;
   dest->pub.term_destination    = term_destination ;
   dest->chunkHead_ = 
   dest->chunkTail_ = 0 ;
}

// returns false if image is too big for buffer
bool imageToJPEG( unsigned char const *inRGB,
                  unsigned             inWidth,
                  unsigned             inHeight,
                  unsigned             inBytes,
                  unsigned char       *outJPEG,
                  unsigned             maxOutBytes,
                  unsigned            &outBytes )
{
   struct jpeg_compress_struct cinfo;
   struct jpeg_error_mgr jerr;
   cinfo.err = jpeg_std_error(&jerr);
   jpeg_create_compress( &cinfo );
   cinfo.in_color_space = JCS_RGB; /* arbitrary guess */
   jpeg_set_defaults(&cinfo);
   cinfo.dct_method = JDCT_IFAST;
   cinfo.in_color_space = JCS_RGB;
   cinfo.input_components = 3;
   cinfo.data_precision = 8;
   cinfo.image_width = (JDIMENSION) inWidth;
   cinfo.image_height = (JDIMENSION) inHeight;
   jpeg_default_colorspace(&cinfo);
   jpeg_mem_dest( &cinfo );
   jpeg_start_compress( &cinfo, TRUE );

   unsigned const row_stride = 3*sizeof(JSAMPLE)*inWidth ; // RGB
   JSAMPARRAY const videoRowBuf = (*cinfo.mem->alloc_sarray)
                                       ( (j_common_ptr)&cinfo,
                                         JPOOL_IMAGE,
                                         row_stride, 1 );
   //
   // loop through scan lines here
   //
   unsigned char const *currentRow = inRGB ;
   unsigned const inStride = inWidth*3 ; // R+G+B
   for( int row = 0 ; row < inHeight ; row++ )
   {
      JSAMPLE *nextOut = videoRowBuf[0];
      for( unsigned col = 0 ; col < inWidth; col++ )
      {
         *nextOut++ = currentRow[2];
         *nextOut++ = currentRow[1];
         *nextOut++ = currentRow[0];
         currentRow += 3 ;
      }
      jpeg_write_scanlines( &cinfo, videoRowBuf, 1 );
   } // for each row

   memChunk_t const *const firstChunk = ( (memDest_t *)cinfo.dest )->chunkHead_ ;
   jpeg_finish_compress( &cinfo );
   memChunk_t const *ch = firstChunk ;
   unsigned totalBytes = 0 ;
   while( ch )
   {
      totalBytes += ch->length_ ;
      ch = ch->next_ ;
   }

   jpeg_destroy_compress( &cinfo );
   if( totalBytes < maxOutBytes )
   {
      JSAMPLE *nextOut = (JSAMPLE *)outJPEG ;
      
      for( ch = firstChunk ; 0 != ch ;  )
      {
         memcpy( nextOut, ch->data_, ch->length_ );
         nextOut += ch->length_ ;
         memChunk_t const *temp = ch ;
         ch = ch->next_ ;
         delete temp ;
      }

      assert( nextOut == (JSAMPLE *)outJPEG + totalBytes );
      outBytes = totalBytes ;
      return true ;
   }
   else
   {
      printf( "JPEG output too big\n" );
      return false ;
   }

}

typedef struct udpHeader_t {
   enum type_e {
      audio     = 0,
      image     = 1,
      barcode   = 2,
      touch     = 3,
      release   = 4,
      pinChange = 5
   };

   unsigned long type_ ;
   unsigned long length_ ;
};

typedef struct gpioPins_t {
   enum {
      carDetect_e    = 1,
      gateOpen_e     = 2,
      guardShack_e   = 4,
   };
   
   unsigned short pinState_ ;
};

typedef struct threadParam_t {
   sockaddr_in remote_ ;
   int         udpSock_ ;
   int         mediaFd_ ;
};

unsigned volatile    audioBytesOut = 0 ;
bool volatile        playingSomething = false ;

static void *audioThread( void *arg )
{
   threadParam_t const &params = *(threadParam_t *)arg ;
   int const fdAudio = params.mediaFd_ ;

   audio_buf_info inf ;
   if( 0 == ioctl( fdAudio, SNDCTL_DSP_GETISPACE, &inf) ) 
   {
      unsigned char *const audioInBuf = new unsigned char[inf.fragsize+sizeof( udpHeader_t)];
      udpHeader_t &header = *( udpHeader_t * )audioInBuf ;
      unsigned char *const readBuf = audioInBuf+sizeof( udpHeader_t );

      do {
         int numRead = read( fdAudio, audioInBuf, inf.fragsize );
         if( inf.fragsize == numRead )
         {
            header.length_ = numRead ;
            header.type_   = header.audio ;
            int numSent = sendto( params.udpSock_, audioInBuf, sizeof(udpHeader_t)+numRead, 0, 
                                  (struct sockaddr *)&params.remote_, sizeof( params.remote_ ) );
            if( numSent == ( sizeof(udpHeader_t)+numRead ) )
            {
               audioBytesOut += numRead ;
            }
            else if( 0 != errno )
            {
               perror( "audioSendTo" );
            }
         }
         else if( 0 != errno )
         {
            fprintf( stderr, "audioRead:%m\n"
                             "audioRead:%m\n"
                             "audioRead:%m\n"
                             "audioRead:%m\n"
                             "audioRead:%m\n"
                             "audioRead:%m\n"
                             "audioRead:%m\n"
                             "audioRead:%m\n"
                      );
         }
      } while( 1 );
   }
   else
      perror( "GETISPACE" );
   
   return 0 ;
}

inline void bail(char const *error){
   perror(error);
   exit(1);
}
#define CHECK(__r,__msg) if(__r) bail(__msg)

static char const *pixfmt_to_str(__u32 pixfmt )
{
   static char cPixFmt[sizeof(pixfmt)+1] = { 0 };
   char const *inchars = (char const *)&pixfmt ;
   memcpy(cPixFmt, inchars, sizeof(pixfmt) );
   return cPixFmt ;
}

// print a fixed-length string (safely)
int printFixed( void const *s, unsigned size )
{
	char *const temp = (char *)alloca(size+1);
	memcpy(temp,s,size);
	temp[size] = 0 ;
	printf( "%s", temp );
}

#define PRINTFIXED(__s) printFixed((__s),sizeof(__s))

#define PRINTCAP(__name)        if( cap.capabilities & __name ) \
                                        printf( "\t\t" #__name  "\n" )
#define CLEAR(__x) memset(&(__x),0,sizeof(__x))

struct map {
    struct v4l2_buffer buf;
    void   *addr;
    size_t len;
};

// returns false if image is too big for buffer
bool bayerToJPEG( unsigned char const *inRGB,
                  unsigned             inWidth,
                  unsigned             inHeight,
                  unsigned             inBytes,
                  unsigned char       *outJPEG,
                  unsigned             maxOutBytes,
                  unsigned            &outBytes )
{
   struct jpeg_compress_struct cinfo;
   struct jpeg_error_mgr jerr;
   cinfo.err = jpeg_std_error(&jerr);
   jpeg_create_compress( &cinfo );
   cinfo.in_color_space = JCS_RGB; /* arbitrary guess */
   jpeg_set_defaults(&cinfo);
   cinfo.dct_method = JDCT_IFAST;
   cinfo.in_color_space = JCS_RGB;
   cinfo.input_components = 3;
   cinfo.data_precision = 8;
   cinfo.image_width = (JDIMENSION) inWidth;
   cinfo.image_height = (JDIMENSION) inHeight;
   jpeg_default_colorspace(&cinfo);
   jpeg_mem_dest( &cinfo );
   jpeg_start_compress( &cinfo, TRUE );

   unsigned const row_stride = 3*sizeof(JSAMPLE)*inWidth ; // RGB
   JSAMPARRAY const videoRowBuf = (*cinfo.mem->alloc_sarray)
                                       ( (j_common_ptr)&cinfo,
                                         JPOOL_IMAGE,
                                         row_stride, 1 );
   //
   // loop through scan lines here
   //
   // see http://www.siliconimaging.com/RGB%20Bayer.htm
   unsigned char const *oddRow  = inRGB ;             // BGBG
   unsigned char const *evenRow = inRGB + inWidth ;   // GRGR

   unsigned const inStride = inWidth ; // R|G|B
   for( int row = 0 ; row < inHeight ; row++ )
   {
      JSAMPLE *nextOut = videoRowBuf[0];
      for( unsigned col = 0 ; col < inWidth; col++ )
      {
         *nextOut++ = 0 ; // evenRow[col|1];
         *nextOut++ = (col&1) ? oddRow[col] : evenRow[col];
         *nextOut++ = 0 ; // oddRow[col&~1];
/*
         *nextOut++ = 0 ;
         *nextOut++ = 0 ;
         *nextOut++ = 0xff ;
*/         
      }
      jpeg_write_scanlines( &cinfo, videoRowBuf, 1 );
      if( row & 1 ){
         oddRow = evenRow + inWidth ;
      } else
         evenRow = oddRow + inWidth ;
/*
*/         
   } // for each row

   memChunk_t const *const firstChunk = ( (memDest_t *)cinfo.dest )->chunkHead_ ;
   jpeg_finish_compress( &cinfo );
   memChunk_t const *ch = firstChunk ;
   unsigned totalBytes = 0 ;
   while( ch )
   {
      totalBytes += ch->length_ ;
      ch = ch->next_ ;
   }

   jpeg_destroy_compress( &cinfo );
   if( totalBytes < maxOutBytes )
   {
      JSAMPLE *nextOut = (JSAMPLE *)outJPEG ;
      
      for( ch = firstChunk ; 0 != ch ;  )
      {
         memcpy( nextOut, ch->data_, ch->length_ );
         nextOut += ch->length_ ;
         memChunk_t const *temp = ch ;
         ch = ch->next_ ;
         delete temp ;
      }

      assert( nextOut == (JSAMPLE *)outJPEG + totalBytes );
      outBytes = totalBytes ;
      return true ;
   }
   else
   {
      printf( "JPEG output too big\n" );
      return false ;
   }

}

static void printBuf(v4l2_buffer const &bufinfo)
{
	printf( "have bufinfo\n" );
	printf( "\tbytesused:\t%u\n", bufinfo.bytesused );
	printf( "\tflags:\t%x\n", bufinfo.flags );
	printf( "\tfield:\t%d\n", bufinfo.field );
	printf( "\tsequence:\t%d\n", bufinfo.sequence );
	printf( "\tlength:\t%u\n", bufinfo.length );
	printf( "\tinput:\t%u\n", bufinfo.input );
}

static void *videoThread( void *arg )
{
   threadParam_t const &params = *( threadParam_t const *)arg ;
   int const fdCamera = params.mediaFd_ ;
   int const udpSock  = params.udpSock_ ;

   struct v4l2_input input;
   int index;
   
   memset (&input, 0, sizeof (input));
   
   while( 1 ){
      if (-1 == ioctl (fdCamera, VIDIOC_ENUMINPUT, &input)) {
              break;
      }
      
      printf ("\tinput %d: %s\n", input.index, input.name);
      input.index++ ;
   }
   
   index = 0;
   if (-1 == ioctl (fdCamera, VIDIOC_S_INPUT, &index)) {
        perror ("VIDIOC_S_INPUT");
        exit (-1);
   }

   printf( "formats:\n" );

   int yuyvFormatIdx = -1 ;
   int bayerFormatIdx = -1 ;
   int jpegFormatIdx = -1 ;
   struct v4l2_fmtdesc fmt ;
   memset( &fmt, 0, sizeof(fmt) );
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
   int pixelFormats[16] = {0};
   while( 1 ){
      if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FMT,&fmt) ){
         fprintf( stderr, "\tfmt:%d:%s %x(%s)\n", fmt.index, fmt.description, fmt.pixelformat, pixfmt_to_str(fmt.pixelformat));
         if( (V4L2_PIX_FMT_JPEG == fmt.pixelformat)
             ||
             (V4L2_PIX_FMT_MJPEG == fmt.pixelformat) ){
            jpegFormatIdx = fmt.index ;
         }
	 else if(V4L2_PIX_FMT_YUYV == fmt.pixelformat){
            yuyvFormatIdx = fmt.index ;
         }
         else if(V4L2_PIX_FMT_SBGGR8 == fmt.pixelformat)
            bayerFormatIdx = fmt.index ;
         pixelFormats[fmt.index] = fmt.pixelformat ;
         fmt.index++ ;
      }
      else {
//            fprintf( stderr, "VIDIOC_ENUM_FMT:%d\n", fmt.index );
         break;
      }
   }

   printf( "yuv index: %d, bayer %d, jpeg: %d\n", yuyvFormatIdx, bayerFormatIdx, jpegFormatIdx );

   struct v4l2_capability cap ;
   if( 0 == ioctl(fdCamera, VIDIOC_QUERYCAP, &cap ) ){
           printf( "have caps:\n" );
           printf( "\tdriver:\t" ); PRINTFIXED(cap.driver); printf( "\n" );
           printf( "\tcard:\t" ); PRINTFIXED(cap.card); printf( "\n" );
           printf( "\tbus:\t" ); PRINTFIXED(cap.bus_info); printf( "\n" );
           printf( "\tversion %u (0x%x)\n", cap.version, cap.version );
           printf( "\tcapabilities: 0x%x\n", cap.capabilities );
           PRINTCAP(V4L2_CAP_VIDEO_CAPTURE);
           PRINTCAP(V4L2_CAP_VIDEO_OUTPUT);
           PRINTCAP(V4L2_CAP_VIDEO_OVERLAY);
           PRINTCAP(V4L2_CAP_VBI_CAPTURE);
           PRINTCAP(V4L2_CAP_VBI_OUTPUT);
           PRINTCAP(V4L2_CAP_SLICED_VBI_CAPTURE);
           PRINTCAP(V4L2_CAP_SLICED_VBI_OUTPUT);
           PRINTCAP(V4L2_CAP_RDS_CAPTURE);
           PRINTCAP(V4L2_CAP_VIDEO_OUTPUT_OVERLAY);
           PRINTCAP(V4L2_CAP_VIDEO_OVERLAY);
           PRINTCAP(V4L2_CAP_TUNER);
           PRINTCAP(V4L2_CAP_AUDIO);
           PRINTCAP(V4L2_CAP_RADIO);
           PRINTCAP(V4L2_CAP_READWRITE);
           PRINTCAP(V4L2_CAP_ASYNCIO);
           PRINTCAP(V4L2_CAP_STREAMING);
   }
   else
           perror("VIDIOC_QUERYCAP" );
   
   v4l2_streamparm streamp ;
   memset(&streamp,0,sizeof(streamp));
   streamp.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
   if( 0 == ioctl(fdCamera, VIDIOC_G_PARM, &streamp)){
      printf( "have stream params\n" );
      printf( "\tparm.capture:\n" );
      printf( "\tparm.capture.capability\t0x%08x\n", streamp.parm.capture.capability );
      printf( "\tparm.capture.capturemode\t0x%08x\n", streamp.parm.capture.capturemode );
      printf( "\tparm.capture.extendedmode\t0x%08x\n", streamp.parm.capture.extendedmode );
      printf( "\tparm.capture.readbuffers\t%u\n", streamp.parm.capture.readbuffers );
      printf( "\tparm.capture.timeperframe\t%u/%u\n"
                    , streamp.parm.capture.timeperframe.numerator
                    , streamp.parm.capture.timeperframe.denominator );
      streamp.parm.capture.timeperframe.numerator = streamp.parm.capture.timeperframe.denominator = 1 ;
      if( 0 == ioctl(fdCamera, VIDIOC_S_PARM, &streamp)){
         printf( "set stream parameters\n" );
      } else
         fprintf( stderr, "VIDIOC_S_PARAM:%m\n" );
   }
   else
          fprintf(stderr,"VIDIOC_G_PARM");

   unsigned rawImageSize = 0 ;
   struct video_capability vidcap ; 
   v4l2_format format ;
   if(0 <= jpegFormatIdx){
          if( 0 > ioctl( fdCamera, VIDIOCGCAP, &vidcap)){
                  perror( "VIDIOCGCAP");
          }
          printf( "JPEG: %ux%u..%ux%u\n", vidcap.minwidth, vidcap.minheight, vidcap.maxwidth, vidcap.maxheight );
          CLEAR(format);
          format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
          format.fmt.pix.field = V4L2_FIELD_NONE ;
          format.fmt.pix.width = vidcap.maxwidth ;
          format.fmt.pix.height = vidcap.maxheight ;
          format.fmt.pix.pixelformat = pixelFormats[jpegFormatIdx];
          format.fmt.pix.bytesperline = format.fmt.pix.width*2 ;
          format.fmt.pix.sizeimage = format.fmt.pix.height*format.fmt.pix.bytesperline ;
          format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB ;
   
          if(0 == ioctl(fdCamera,VIDIOC_S_FMT,&format)){
                  printf( "set JPEG capture: pixel format %08x\n", pixelFormats[jpegFormatIdx] );
          }
          else
                  perror("VIDIOC_S_FMT");
   
          if(0 == ioctl(fdCamera,VIDIOC_G_FMT,&format)){
                  printf( "JPEG capture data:\n" );
                  printf( "\t%ux%u.. %ubpl... %ubpp\n", format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.bytesperline, format.fmt.pix.sizeimage );
                  rawImageSize = format.fmt.pix.sizeimage ;
          }
          else
                  perror("VIDIOC_S_FMT");
   } // supports JPEG
   else if(0 <= yuyvFormatIdx){
          if( 0 > ioctl( fdCamera, VIDIOCGCAP, &vidcap)){
                  perror( "VIDIOCGCAP");
          }
          printf( "%ux%u..%ux%u\n", vidcap.minwidth, vidcap.minheight, vidcap.maxwidth, vidcap.maxheight );
          CLEAR(format);
          format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
          format.fmt.pix.field = V4L2_FIELD_NONE ;
          format.fmt.pix.width = vidcap.maxwidth ;
          format.fmt.pix.height = vidcap.maxheight ;
          format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV ;
          format.fmt.pix.bytesperline = format.fmt.pix.width*2 ;
          format.fmt.pix.sizeimage = format.fmt.pix.height*format.fmt.pix.bytesperline ;
          format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB ;
   
          if(0 == ioctl(fdCamera,VIDIOC_S_FMT,&format)){
                  printf( "set YUYV capture\n" );
          }
          else
                  perror("VIDIOC_S_FMT");
   
          if(0 == ioctl(fdCamera,VIDIOC_G_FMT,&format)){
                  printf( "YUYV capture data:\n" );
                  printf( "\t%ux%u.. %ubpl... %ubpp\n", format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.bytesperline, format.fmt.pix.sizeimage );
                  rawImageSize = format.fmt.pix.sizeimage ;
          }
          else
                  perror("VIDIOC_S_FMT");
   } // supports YUYV
   else if( 0 <= bayerFormatIdx ){
      if( 0 > ioctl( fdCamera, VIDIOCGCAP, &vidcap)){
            perror( "VIDIOCGCAP");
      }
      printf( "%ux%u..%ux%u\n", vidcap.minwidth, vidcap.minheight, vidcap.maxwidth, vidcap.maxheight );
      CLEAR(format);
      format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
      format.fmt.pix.field = V4L2_FIELD_NONE ;
      format.fmt.pix.width = vidcap.maxwidth ;
      format.fmt.pix.height = vidcap.maxheight ;
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR8 ;
//      format.fmt.pix.bytesperline = format.fmt.pix.width*2 ;
//      format.fmt.pix.sizeimage = format.fmt.pix.height*format.fmt.pix.bytesperline ;
//      format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB ;
      
      if(0 == ioctl(fdCamera,VIDIOC_S_FMT,&format)){
            printf( "set YUYV capture\n" );
      }
      else
            perror("VIDIOC_S_FMT");
      
      if(0 == ioctl(fdCamera,VIDIOC_G_FMT,&format)){
            printf( "bayer capture data:\n" );
            printf( "\t%ux%u.. %ubpl... %ubpp\n", format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.bytesperline, format.fmt.pix.sizeimage );
            rawImageSize = format.fmt.pix.sizeimage ;
      }
      else
            perror("VIDIOC_S_FMT");
   } // supports bayer format
   else {
      printf( "No video stream!\n" );
      return 0 ;
   }

   struct v4l2_frmsizeenum frameSize ;
   CLEAR(frameSize);
   frameSize.pixel_format = (0 < yuyvFormatIdx) ? V4L2_PIX_FMT_YVU420 
                                                : V4L2_PIX_FMT_SBGGR8 ;
   while(1){
          if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FRAMESIZES,&frameSize) ){
                  printf( "have frame size %d\n", frameSize.index );
                  frameSize.index++ ;
          }
          else {
                  printf( "No more frame sizes: %m\n" );
                  break;
          }
   }
   
   struct v4l2_frmivalenum frameIval ;
   CLEAR(frameIval);
   frameIval.pixel_format = V4L2_PIX_FMT_YVU420 ;
   while(1){
          if( 0 == ioctl(fdCamera,VIDIOC_ENUM_FRAMESIZES,&frameSize) ){
                  printf( "have frame ival %d\n", frameSize.index );
                  frameSize.index++ ;
          }
          else {
                  printf( "No more frame ivals: %m\n" );
                  break;
          }
   }
   unsigned const numBuffers = 8 ;
   v4l2_requestbuffers rb ;
   memset(&rb,0,sizeof(rb));
   rb.count = numBuffers ;
   rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE ;
   rb.memory = V4L2_MEMORY_MMAP ;
   if( 0 == ioctl(fdCamera, VIDIOC_REQBUFS, &rb) ){
          printf( "have %u bufs (requested %u)\n", rb.count, numBuffers );
   }
   else
          perror( "VIDIOC_REQBUFS" );

   struct map * const bufferMaps = new struct map [numBuffers];
   
   for (unsigned i = 0; i < numBuffers; ++i ) {
          struct map &m = bufferMaps[i];
          CLEAR(bufferMaps[i]);
          m.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          m.buf.memory = V4L2_MEMORY_MMAP;
          m.buf.index = i;
          if (0 >ioctl(fdCamera, VIDIOC_QUERYBUF, bufferMaps+i)) {
                  fprintf( stderr, "VIDIOC_QUERYBUF(%u):%m\n", i );
                  return 0 ;
          }
          printBuf(m.buf);
          m.len = m.buf.length ;
          m.addr = mmap(NULL,m.len, PROT_READ, MAP_SHARED, fdCamera, m.buf.m.offset);
          if( (0==m.addr) || (MAP_FAILED==m.addr) ){
                  fprintf(stderr, "MMAP(buffer %u):%u bytes:%m\n", i, m.len );
                  return 0 ;
          }
          if (-1 == ioctl(fdCamera, VIDIOC_QBUF, bufferMaps+i)){
                  perror("VIDIOC_QBUF");
                  return 0 ;
          }
   }
   v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   if (-1 == ioctl(fdCamera, VIDIOC_STREAMON, &type)){
          perror("VIDIOC_STREAMON");
          return 0 ;
   }
   
   printf( "queued %u buffers\n", numBuffers );
   unsigned iterations = 0 ;
   unsigned char *jpegBuf = (0 <= jpegFormatIdx) 
                            ? 0
                            : new unsigned char [rawImageSize];
   fbDevice_t &fb = getFB();
   int first = 1 ;

   while(1) {
      struct v4l2_buffer buf ;
      CLEAR(buf);
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      if(-1 == ioctl(fdCamera, VIDIOC_DQBUF, &buf)) {
         switch(errno) {
            case EAGAIN:
               printf( "wait...\n" );
               break ;
            case EIO:
               /* Could ignore EIO, see spec. */
               /* fall through */

               default:
               perror("VIDIOC_DQBUF");
               return 0 ;
         }
      }
      else {
//         printf("%u: %u\n", iterations++, buf.index );
//         printBuf(buf);
         if( first ){
            first = false ;
            FILE *fOut = fopen( "/tmp/img.out", "wb" );
            if( fOut ){
               fwrite( (void *)bufferMaps[buf.index].addr, 1, buf.bytesused, fOut );
               fclose( fOut );
            }
         }

         void const *rgb565 = 0 ;
         unsigned short width ;
         unsigned short height ;
         unsigned jpegBytes = 0 ;
         
         if( 0 <= jpegFormatIdx ){
            width = format.fmt.pix.width ;
            height = format.fmt.pix.height ;
            jpegBuf = (unsigned char *)bufferMaps[buf.index].addr ;
            jpegBytes = buf.bytesused ;
/*
            if( imageJPEG(jpegBuf, jpegBytes, rgb565, width, height) ){
               printf( "decoded\n" );
            }
            else
               fprintf( stderr, "decode error\n" );
*/               
         } else if( 0 <= yuyvFormatIdx ){
            printf( "convert to JPEG from YUYV here\n" );
         } else {
            if( bayerToJPEG( (unsigned char const *)bufferMaps[buf.index].addr, 
                             format.fmt.pix.width, 
                             format.fmt.pix.height, rawImageSize,
                             jpegBuf, rawImageSize, jpegBytes ) ){
               printf( "converted\n" );
   
               if( imageJPEG(jpegBuf, jpegBytes, rgb565, width, height) ){
                  printf( "decoded\n" );
               }
               else
                  fprintf( stderr, "decode error\n" );
            }
            else
               fprintf( stderr, "JPEG compress error\n" );
         } // bayer form
         
         if( rgb565 ){
            fb.render( 0, 0, width, height, (unsigned short const *)rgb565 );
            delete [] (unsigned short *)rgb565 ;
         }

         if( jpegBuf && (0 < jpegBytes) ){
            int const numSent = sendto( udpSock, jpegBuf, jpegBytes, 0, (struct sockaddr *)&params.remote_, sizeof( params.remote_ ) );
            if( numSent == jpegBytes )
               printf( "sent: %lu %u %lu bytes\n", iterations++, buf.index, jpegBytes );
            else
               fprintf( stderr, "send error: %d of %u:%m\n", numSent, jpegBytes );
         }

         if(0 > ioctl(fdCamera, VIDIOC_QBUF, &buf)) {
            perror("VIDIOC_QBUF");
            return 0 ;
         }
      }
   }
}

struct deviceMsg_t {
   enum type_e {
      audio_e  = 0,
      unlock_e = 1,
      lock_e   = 2,
      reject_e = 3
   };

   enum {
      MAXSAMPLES = 0x2000
   };
   type_e         type_ ;
   unsigned long  length_ ; // in bytes
   unsigned short data_[MAXSAMPLES];
};



static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
}


#define VIDEO_PALETTE_BAYER	(('q'<<8) | 1)	/* Grab video in raw Bayer format */
#define VIDEO_PALETTE_MJPEG	(('q'<<8) | 2)	/* Grab video in compressed MJPEG format */

int main( int argc, char const * const argv[] )
{
   signal( SIGINT, ctrlcHandler );

   if( 4 == argc )
   {
      int const targetIP = inet_addr( argv[1] );
      if( INADDR_NONE != targetIP )
      {
         int const localIP = inet_addr( argv[2] );
         if( INADDR_NONE != localIP )
         {
            unsigned short const targetPort = strtoul( argv[3], 0, 0 );
            sockaddr_in remote ;
            remote.sin_family      = AF_INET ;
            remote.sin_addr.s_addr = targetIP ;
            remote.sin_port        = htons(targetPort) ;
   
            int mixerFd = open( "/dev/mixer", O_RDWR );
            if( 0 <= mixerFd )
            {
               int recordLevel = 1 ;
               if( 0 == ioctl( mixerFd, MIXER_WRITE( SOUND_MIXER_MIC ), &recordLevel ) )
                  printf( "record boost is now %d\n", recordLevel );
               else
                  perror( "set boost" );
   
               recordLevel = 0x6464 ;
               if( 0 == ioctl( mixerFd, MIXER_READ( SOUND_MIXER_IGAIN ), &recordLevel ) )
                  printf( "igain was %d\n", recordLevel );
               else
                  perror( "get igain" );
   
               close( mixerFd );
            }
            else
               fprintf( stderr, "Error opening mixer\n" );
   
            int fdAudio = open( "/dev/dsp", O_RDWR );
            if( 0 <= fdAudio )
            {
               printf( "opened /dev/dsp\n" );
               if( 0 == ioctl( fdAudio, SNDCTL_DSP_SYNC, 0 ) ) 
               {
                  int const format = AFMT_S16_LE ;
                  if( 0 == ioctl( fdAudio, SNDCTL_DSP_SETFMT, &format) ) 
                  {
                     int const channels = 1 ;
                     if( 0 != ioctl( fdAudio, SNDCTL_DSP_CHANNELS, &channels ) )
                        fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS)\n" );
         
                     int speed = 11025 ;
                     while( 0 < speed )
                     {
                        if( 0 == ioctl( fdAudio, SNDCTL_DSP_SPEED, &speed ) )
                        {
                           printf( "set speed to %u\n", speed );
                           break;
                        }
                        else
                           fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", speed );
                        speed /= 2 ;
                     }
                  
                     int dont = 0 ;
                     if( 0 != ioctl( fdAudio, SNDCTL_DSP_STEREO, &dont ) )
                        perror( "STEREO" );
   
                     int const vol = 0x6464 ;
                     if( 0 > ioctl( fdAudio, SOUND_MIXER_WRITE_VOLUME, &vol)) 
                        perror( "Error setting volume" );
   
                     if( 0 < speed )
                     {
                        close( fdAudio );
                        fdAudio = open( "/dev/dsp", O_RDONLY );
                        int fdAudioWrite = open( "/dev/dsp", O_WRONLY );
   
                        if( 0 > ioctl( fdAudioWrite, SOUND_MIXER_WRITE_VOLUME, &vol)) 
                           perror( "Error setting volume" );
                        else
                           printf( "set volume to %u\n", vol & 0xFF );
   
                        if( 0 != ioctl( fdAudioWrite, SNDCTL_DSP_SETFMT, &format) ) 
                           fprintf( stderr, "DSP_SETFMT:%m\n" );
   
                        if( 0 != ioctl( fdAudioWrite, SNDCTL_DSP_CHANNELS, &channels ) )
                           fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS)\n" );
            
                        if( 0 != ioctl( fdAudioWrite, SNDCTL_DSP_SPEED, &speed ) )
                           fprintf( stderr, "set speed %m\n" );
                     
                        if( 0 != ioctl( fdAudioWrite, SNDCTL_DSP_STEREO, &dont ) )
                           perror( "STEREO" );
   
                        int fdCamera = open( "/dev/video0", O_RDONLY );
                        if( 0 <= fdCamera )
                        {
                           int const udpSock = multicastSocket(targetIP, localIP);
                           if( 0 <= udpSock )
                           {
                              int rval = fcntl(udpSock, F_SETFL, fcntl(udpSock, F_GETFL,0) | O_NONBLOCK);
                              if( rval )
                                 perror("F_SETFL");
                              else
                                 printf( "----------> set non-blocking\n" );

                              threadParam_t videoParams ; 
                              videoParams.remote_ = remote ;
                              videoParams.mediaFd_ = fdCamera ;
                              videoParams.udpSock_ = udpSock ;
                              
                              pthread_t videoHandle ;
                              int create = pthread_create( &videoHandle, 0, videoThread, &videoParams );
                              if( 0 == create )
                              {
                                 printf( "threads created... <ctrl-c> to stop\n" );
                                 pause();
                                 printf( "shutting down\n" );
                                 pthread_cancel( videoHandle );
                                 void *exitStat ;
                                 pthread_join( videoHandle, &exitStat );
                              }
   			}
   			else
   				perror( "udpSock" );
                        }
                        else
                           perror( "/dev/video0" );
                     }
                  }
                  else
                     printf( "Error setting audio format\n" );
               }
               close( fdAudio );
            }
         }
         else
            printf( "Invalid local IP address %s\n", argv[2] );
      }
      else
         printf( "Invalid target IP address %s\n", argv[1] );
   }
   else
      printf( "Usage: %s targetip(dotted decimal) port\n", argv[0] );

   return 0 ;
}
