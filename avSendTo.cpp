/*
 * Module avSendTo.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: avSendTo.cpp,v $
 * Revision 1.2  2003-09-22 02:07:35  ericn
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
#include "imgJPEG.h"
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
      audio = 0,
      image = 1
   };

   unsigned long type_ ;
   unsigned long length_ ;
};

typedef struct threadParam_t {
   sockaddr_in remote_ ;
   int         udpSock_ ;
   int         mediaFd_ ;
};

unsigned volatile audioBytesOut = 0 ;

static void *audioThread( void *arg )
{
   threadParam_t const &params = *(threadParam_t *)arg ;
   int const fdAudio = params.mediaFd_ ;

   audio_buf_info inf ;
   if( 0 == ioctl( fdAudio, SNDCTL_DSP_GETISPACE, &inf) ) 
   {
printf( "inf.bytes = %u\n", inf.bytes );
printf( "inf.fragments = %u\n", inf.fragments );
printf( "inf.fragsize = %u\n", inf.fragsize );
printf( "inf.fragstotal = %u\n", inf.fragstotal );

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
//               printf( "audio %u, %u\n", numRead, audioBytesOut );
            }
            else
            {
               perror( "audioSendTo" );
               break;
            }
         }
         else
         {
            perror( "audioRead" );
            break;
         }
      } while( 1 );
   }
   else
      perror( "GETISPACE" );
   
   return 0 ;
}

static void *videoThread( void *arg )
{
   threadParam_t const &params = *( threadParam_t const *)arg ;
   int const fdCamera = params.mediaFd_ ;
   int const udpSock  = params.udpSock_ ;

   struct video_picture    vidpic ; 
   ioctl( fdCamera, VIDIOCGPICT, &vidpic);
//                        vidpic.palette = VIDEO_PALETTE_RGB24 ;
//                        vidpic.palette = VIDEO_PALETTE_GREY;	
//                        vidpic.palette = VIDEO_PALETTE_HI240;	
//                        vidpic.palette = VIDEO_PALETTE_RGB565;	
   vidpic.palette = VIDEO_PALETTE_RGB24;	
//                        vidpic.palette = VIDEO_PALETTE_RGB32;	
//                        vidpic.palette = VIDEO_PALETTE_RGB555;	
//                        vidpic.palette = VIDEO_PALETTE_YUV422	;
//                        vidpic.palette = VIDEO_PALETTE_YUYV	;
//                        vidpic.palette = VIDEO_PALETTE_UYVY	;
//                        vidpic.palette = VIDEO_PALETTE_YUV420	;
//                        vidpic.palette = VIDEO_PALETTE_YUV411	;
//                        vidpic.palette = VIDEO_PALETTE_RAW	;
//                        vidpic.palette = VIDEO_PALETTE_YUV422P	;
//                        vidpic.palette = VIDEO_PALETTE_YUV411P	;
//                        vidpic.palette = VIDEO_PALETTE_YUV420P	;
//                        vidpic.palette = VIDEO_PALETTE_YUV410P	;
//                        vidpic.palette = VIDEO_PALETTE_PLANAR	;
//                        vidpic.palette = VIDEO_PALETTE_COMPONENT;
//                        vidpic.palette = VIDEO_PALETTE_BAYER ;
   ioctl( fdCamera, VIDIOCSPICT, &vidpic);

   struct video_capability vidcap ; 
   ioctl( fdCamera, VIDIOCGCAP, &vidcap);

   struct video_window     vidwin ;
   ioctl( fdCamera, VIDIOCGWIN, &vidwin);
   vidwin.width  = vidcap.maxwidth ;
   vidwin.height = vidcap.maxheight ;
   ioctl( fdCamera, VIDIOCSWIN, &vidwin);
printf( "camera: %u x %u\n", vidwin.width, vidwin.height );
   unsigned const bufSize = vidwin.height*vidwin.width * 3 ;
   struct video_mbuf vidbuf;
   if( 0 <= ioctl( fdCamera, VIDIOCGMBUF, &vidbuf) )
   { 
      unsigned char *const vidBuf = new unsigned char [bufSize];
      unsigned const maxJPEG = bufSize/2 ;
      unsigned char *const imageBuf = new unsigned char [maxJPEG];
      udpHeader_t &header = *( udpHeader_t * )imageBuf ;
      unsigned char *const jpegBuf = imageBuf+sizeof( udpHeader_t );
      unsigned numJPEGBytes ;
      
      if( ( 0 != vidBuf ) && ( 0 != imageBuf ) )
      {
         time_t const start = time( 0 );
         time_t prevSec = time( 0 );
         unsigned prevFrames  = 0 ;
         unsigned frameCnt = 0 ;
         do {
            int numRead = read( fdCamera, vidBuf, bufSize );

            if( numRead == bufSize )
            {
               if( imageToJPEG( vidBuf, 
                                vidwin.width, 
                                vidwin.height,
                                bufSize,
                                jpegBuf,
                                maxJPEG,
                                numJPEGBytes ) )
               {
                  header.length_ = numJPEGBytes ;
                  header.type_ = header.image ;
                  int const numSent = sendto( udpSock, imageBuf, sizeof( udpHeader_t)+numJPEGBytes, 0, (struct sockaddr *)&params.remote_, sizeof( params.remote_ ) );
                  if( numSent == sizeof( udpHeader_t)+numJPEGBytes )
                  {
                     frameCnt++ ;
                     time_t const now = time( 0 );
                     if( now != prevSec )
                     {
                        unsigned const deltaFrames = frameCnt-prevFrames ;
                        prevFrames = frameCnt ;
                        prevSec   = now ;
                        unsigned const deltaSecs = now-start ;
                        printf( "\r%u fps, %u BPS audio   ", deltaFrames, audioBytesOut/deltaSecs ); fflush( stdout );
                     }
                  }
                  else
                     perror( "sendto" );
               }
               else
                  printf( "jpegError\n" );
            }
            else
               printf( "mismatch:%u/%u\n", bufSize, numRead );
         } while( 1 ); // time( 0 )-start < 10 );
         
         printf( "%u frames\n", frameCnt );
         if( 0 < frameCnt )
         {
            FILE *fOut = fopen( "image.jpg", "wb" );
            if( fOut )
            {   
               fwrite( jpegBuf, numJPEGBytes, 1, fOut );
               fclose( fOut );
            }
            hexDumper_t dumpRGB( vidBuf, 256 );
            while( dumpRGB.nextLine() )
               printf( "%s\n", dumpRGB.getLine() );
         }
      }
      else
         perror( "mmap" );
   }
   else
      perror( "VIDIOCGMBUF" );
}

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
}

#define VIDEO_PALETTE_BAYER	(('q'<<8) | 1)	/* Grab video in raw Bayer format */
#define VIDEO_PALETTE_MJPEG	(('q'<<8) | 2)	/* Grab video in compressed MJPEG format */

int main( int argc, char const * const argv[] )
{
   signal( SIGINT, ctrlcHandler );

   if( 3 == argc )
   {
      int const targetIP = inet_addr( argv[1] );
      if( INADDR_NONE != targetIP )
      {
         unsigned short const targetPort = strtoul( argv[2], 0, 0 );
         sockaddr_in remote ;
         remote.sin_family      = AF_INET ;
         remote.sin_addr.s_addr = targetIP ;
         remote.sin_port        = targetPort ;

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

         int fdAudio = open( "/dev/dsp", O_RDONLY );
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
               
                  if( 0 < speed )
                  {
                     int fdCamera = open( "/dev/video0", O_RDONLY );
//                     if( 0 <= fdCamera )
                     {
                        int const udpSock = socket( AF_INET, SOCK_DGRAM, 0 );
                        if( 0 <= udpSock )
                        {
                           threadParam_t audioParams ; 
                           audioParams.remote_ = remote ;
                           audioParams.mediaFd_ = fdAudio ;
                           audioParams.udpSock_ = udpSock ;
                           pthread_t audioHandle ;
                           int create = pthread_create( &audioHandle, 0, audioThread, &audioParams );
                           if( 0 == create )
                           {
                              if( 0 <= fdCamera )
                              {
                                 threadParam_t videoParams ; 
                                 videoParams.remote_ = remote ;
                                 videoParams.mediaFd_ = fdCamera ;
                                 videoParams.udpSock_ = udpSock ;
   
                                 pthread_t videoHandle ;
                                 create = pthread_create( &videoHandle, 0, videoThread, &videoParams );
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
                              {   
                                 printf( "no video... sending audio\n" );
                                 pause();
                              }

                              void *exitStat ;
                              pthread_cancel( audioHandle );
                              pthread_join( audioHandle, &exitStat );
                           }
                           else
                              perror( "aThread" );
                        }
                        else
                           perror( "udpSock" );
                     }
//                     else
//                        perror( "/dev/video0" );
                  }
               }
               else
                  printf( "Error setting audio format\n" );
            }
            close( fdAudio );
         }
      }
      else
         printf( "Invalid target IP address %s\n", argv[1] );
   }
   else
      printf( "Usage: %s targetip(dotted decimal) port\n", argv[0] );

   return 0 ;
}
