/*
 * Module avSendTo.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: avSendTo.cpp,v $
 * Revision 1.17  2003-11-04 00:40:12  tkisky
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
#include "barcodePoll.h"
#include "touchPoll.h"
#include "tickMs.h"
#include "gpioPoll.h"
#include "fbDev.h"
#include <errno.h>
#include "imgJPEG.h"
#include "memFile.h"
#include "imgFile.h"

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
gpioPins_t volatile  gpioPins_ = { 0 };

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

static void *videoThread( void *arg )
{
   threadParam_t const &params = *( threadParam_t const *)arg ;
   int const fdCamera = params.mediaFd_ ;
   int const udpSock  = params.udpSock_ ;

   struct video_picture    vidpic ; 
   ioctl( fdCamera, VIDIOCGPICT, &vidpic);
   vidpic.palette = VIDEO_PALETTE_RGB24;	
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
                  unsigned const numToSend = sizeof( udpHeader_t)+numJPEGBytes ;
                  int const numSent = sendto( udpSock, imageBuf, numToSend, 0, (struct sockaddr *)&params.remote_, sizeof( params.remote_ ) );
                  if( numSent == numToSend )
                  {
                     frameCnt++ ;
/*
                     time_t const now = time( 0 );
                     if( now != prevSec )
                     {
                        unsigned const deltaFrames = frameCnt-prevFrames ;
                        prevFrames = frameCnt ;
                        prevSec   = now ;
                        unsigned const deltaSecs = now-start ;
                        printf( "\r%u fps, %u BPS audio   ", deltaFrames, audioBytesOut/deltaSecs ); fflush( stdout );
                     }
*/                     
                  }
                  else
                     fprintf( stderr, "%m sending image: %u/%u\n", numSent, numToSend );
               }
               else
                  printf( "jpegError\n" );
            }
            else
            {
               printf( "mismatch:%u/%u\n", bufSize, numRead );
               if( -1 == numRead )
               {
                  fprintf( stderr, "Invalid data from camera:%d:%m\n", errno );
                  break;
               }
            }
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

//
// this class represents the U/I state for the 
//
// Note that there are probably six elements which
// could be used to drive the display :
//
//    lock/unlock                - set+cleared by udpRxThread
//    Touch/not                  - set+cleared by touchPoll
//    barcode pending/not        - set+cleared by barcodePoll cleared by 
//                                 udpRxThread(unlock or reject) or timeout
//    open/not                   - set+cleared by gpioPoll
//    barcode reject/not         - set by udpRxThread, cleared by 
//                                 udpRxThread(unlock) or timeout
//    car present/not            - set/cleared by gpio poll handler
//
// For now, we're only using three:
//
//    lock/not    - depending on whether we're trying to open the gate
//    open/not    - depending on whether the gate remains open
//    touch/not   - depending on whether the screen is touched
// 
class uiState_t {
public:
   enum input_e {
      lock_e         = 1,
      gateOpen_e     = 2,
      touch_e        = 4,
      bcReject_e     = 8,
      bcPending_e    = 16,
      carPresent_e   = 32
   };

   uiState_t( void );
   ~uiState_t( void ){}

   void setState( input_e input,
                  bool    setNotCleared );

   unsigned char getState( void ) const { return state_ ; }

private:
   unsigned char state_ ;
   image_t const *images_[4];
   image_t        welcomeImage_ ;
   image_t        unlockImage_ ;
   image_t        openImage_ ;
};


uiState_t :: uiState_t( void )
   : state_( 0 )
{
   if( !imageFromFile( "logo.jpg", welcomeImage_ ) )
      fprintf( stderr, "Error reading welcome logo\n" );
   if( !imageFromFile( "unlock.jpg", unlockImage_ ) )
      fprintf( stderr, "Error reading unlocked image\n" );
   if( !imageFromFile( "open.gif", openImage_ ) )
      fprintf( stderr, "Error reading open image\n" );
   
   images_[0]           = &unlockImage_ ;
   images_[lock_e]      = &welcomeImage_ ;
   images_[gateOpen_e]  = &openImage_ ;
   images_[lock_e
          |gateOpen_e]  = &openImage_ ;
   setState( lock_e, true );
}

static void invertScreen( void )
{
   fbDevice_t &fb = getFB();
   unsigned long *fbMem = (unsigned long *)fb.getMem();
   unsigned long longs = fb.getMemSize()/sizeof(fbMem[0]);

   while( 0 < longs-- )
   {
      *fbMem = ~(*fbMem);
      fbMem++ ;
   }
}

static int const waveDataStart = 64 ; // offset of might be off, but no matter

void uiState_t :: setState
   ( input_e input,
     bool    setNotCleared )
{
   unsigned char const imgBits  = lock_e | gateOpen_e ;
   unsigned char const oldState = state_ ;
   if( setNotCleared )
      state_ |= input ;
   else
      state_ &= ~input ;

   if( ( oldState & imgBits ) != ( state_ & imgBits ) )
   {
      image_t const &image = *images_[state_ & imgBits];
      if( image.pixData_ )
         getFB().render( 0, 0, image.width_, image.height_, (unsigned short *)image.pixData_ );
      if( state_ & touch_e )
      {
         invertScreen();
      }
   }
   else if( ( state_ & touch_e ) != ( oldState & touch_e ) )
   {
      invertScreen();
   }
}



class udpBarcode_t : public barcodePoll_t {
public:
   udpBarcode_t( pollHandlerSet_t  &set,
                 int                udpSock,
                 sockaddr_in const &remote,
                 int                audioFd );

   virtual void onBarcode( void );

   enum scannerType_e {
      sick_e,
      keyence_e,
      other_e
   };

private:
   int            udpSock_ ;
   sockaddr_in    remote_ ;
   scannerType_e  scannerType_ ;
   unsigned long  prevTick_ ;
   char           prevBarcode_[256];
   int      const audioFd_ ;
   unsigned char *beepData_ ;
   unsigned       beepBytes_ ;
};

static int strTableLookup( char const * const strTable[],
                           unsigned           nelements,
                           char const        *target )
{
   for( unsigned i = 0 ; i < nelements ; i++ )
   {
      if( 0 == strcasecmp( strTable[i], target ) )
         return i ;
   }

   return -1 ;
}

udpBarcode_t :: udpBarcode_t
   ( pollHandlerSet_t  &set,
     int                udpSock,
     sockaddr_in const &remote,
     int                audioFd )
   : barcodePoll_t( set )
   , udpSock_( udpSock )
   , remote_( remote )
   , scannerType_( sick_e )
   , prevTick_( 0 )
   , audioFd_( audioFd )
   , beepData_( 0 )
   , beepBytes_( 0 )
{
   static char const *const scannerTypes[] = {
      "sick", "keyence", "other"
   };

   int type = -1 ;

   FILE *fScannerType = fopen( "/js/scannerType", "rb" );
   if( fScannerType )
   {
      char inBuf[256];
      int numRead = fread( inBuf, 1, sizeof( inBuf )-1, fScannerType );
      if( 0 < numRead )
      {
         inBuf[numRead] = '\0' ;
         type = strTableLookup( scannerTypes, 3, inBuf );
      }
      fclose( fScannerType );
   }
   
   char const scannerTerminators[] = {
      '\x03', '\r', '\0'
   };
   if( 0 > type )
   {
      printf( "undefined scanner type... defaulting to sick\n" );
      scannerType_ = sick_e ;  // default to sick
   }
   else
      scannerType_ = (scannerType_e)type ;

   terminator_ = scannerTerminators[type];

   if( other_e == type ) // Symbol scanners don't beep
   {   
      int fdBeep = open( "badumm.wav", O_RDONLY );
      if( 0 <= fdBeep )
      {
         long eof = lseek( fdBeep, 0, SEEK_END );
         if( 64 < eof )
         {
            beepData_  = new unsigned char[ eof-64 ];
            beepBytes_ = (unsigned)eof ;
            lseek( fdBeep, waveDataStart, SEEK_SET );
            read( fdBeep, beepData_, beepBytes_ );
         }
         close( fdBeep );
      }
      else
         perror( "beep" );
   }
}

void udpBarcode_t :: onBarcode( void )
{
   unsigned len = strlen( getBarcode() );
//   printf( "barcode <%s>, len %u\n", getBarcode(), len );

   if( sick_e == scannerType_ )
   {
      if( '\x03' == barcode_[--len] ) 
      {
         barcode_[--len] = '\0' ; // trim symbology
      }
   }

   unsigned long const now = tickMs();
   if( ( 0 != strcmp( barcode_, prevBarcode_ ) )
       ||
       ( 1000 < now-prevTick_ ) )
   {
      char data[sizeof(udpHeader_t)+sizeof(barcode_)];
      udpHeader_t &header = *(udpHeader_t *)data ;
      header.type_   = header.barcode ;
      header.length_ = len + 1 ; // include trailing NULL
      strcpy( data+sizeof(header), getBarcode() );
   
      unsigned numToSend = sizeof(udpHeader_t)+header.length_ ;
      int numSent = sendto( udpSock_, data, numToSend, 0, 
                            (struct sockaddr *)&remote_, sizeof( remote_ ) );
      if( numSent != numToSend )
         perror( "barcode sendto" );
      strcpy( prevBarcode_, barcode_ );
      prevTick_ = now ;
      
      if( ( 0 != beepData_ ) && ( 0 != beepBytes_ ) )
      {
         write( audioFd_, beepData_, beepBytes_ );
      } // play beep sound
   }
   else
      printf( "duplicate barcode %s ignored\n", barcode_ );
}

class udpTouch_t : public touchPoll_t {
public:
   udpTouch_t( pollHandlerSet_t  &set,
               int                udpSock,
               sockaddr_in const &remote,
               int                audioOutFd );

   // override this to perform processing of a touch
   virtual void onTouch( int x, int y, unsigned pressure, timeval const &tv );
   // override this to perform processing of a release
   virtual void onRelease( timeval const &tv );

private:
   int            udpSock_ ;
   sockaddr_in    remote_ ;
   bool           first_ ;
   bool           wasTouched_ ;
   long long      prevTime_ ;
   int            audioFd_ ;
   unsigned char *dingData_ ;
   unsigned       dingBytes_ ;
   unsigned char *dongData_ ;
   unsigned       dongBytes_ ;
};

udpTouch_t :: udpTouch_t
   ( pollHandlerSet_t  &set,
     int                udpSock,
     sockaddr_in const &remote,
     int                audioOutFd )
   : touchPoll_t( set )
   , udpSock_( udpSock )
   , remote_( remote )
   , first_( true )
   , wasTouched_( false )
   , prevTime_( 0LL )
   , audioFd_( audioOutFd )
   , dingData_( 0 )
   , dingBytes_( 0 )
   , dongData_( 0 )
   , dongBytes_( 0 )
{
   int fdDing = open( "ding.wav", O_RDONLY );
   if( 0 <= fdDing )
   {
      long eof = lseek( fdDing, 0, SEEK_END );
      if( 64 < eof )
      {
         dingData_  = new unsigned char[ eof-64 ];
         dingBytes_ = (unsigned)eof ;
         lseek( fdDing, waveDataStart, SEEK_SET );
         read( fdDing, dingData_, dingBytes_ );
      }
      close( fdDing );
   }
   else
      perror( "ding" );

   int fdDong = open( "dong.wav", O_RDONLY );
   if( 0 <= fdDong )
   {
      long eof = lseek( fdDong, 0, SEEK_END );
      if( 64 < eof )
      {
         dongData_  = new unsigned char[ eof-64 ];
         dongBytes_ = (unsigned)eof ;
         lseek( fdDong, waveDataStart, SEEK_SET );
         read( fdDong, dongData_, dongBytes_ );
      }
      close( fdDong );
   }
   else
      perror( "dong" );
}

void udpTouch_t :: onTouch( int x, int y, unsigned pressure, timeval const &tv )
{
   bool send ;
   if( first_ || ( !wasTouched_ ) )
   {
      ioctl( audioFd_, SNDCTL_DSP_RESET, 0 );
      invertScreen();
      wasTouched_ = true ;
      send = true ; // first press
      if( 0 < dingBytes_ )
         write( audioFd_, dingData_, dingBytes_ );
      first_ = false ;
   }
   else 
   {
      long long nowMs = tickMs();
      send = ( nowMs - prevTime_ ) > 500 ;
   }

   if( send )
   {
      udpHeader_t header ;
      header.type_ = header.touch ;
      header.length_ = 0 ;
      int numSent = sendto( udpSock_, &header, sizeof( header ), 0, 
                            (struct sockaddr *)&remote_, sizeof( remote_ ) );
      if( sizeof( header ) != numSent )
         perror( "touch sendto" );
      prevTime_ = tickMs();
   }
}

void udpTouch_t :: onRelease( timeval const &tv )
{
   if( wasTouched_ )
   {
      invertScreen();
      ioctl( audioFd_, SNDCTL_DSP_RESET, 0 );
      if( 0 < dongBytes_ )
         write( audioFd_, dongData_, dongBytes_ );
      wasTouched_ = false ;
      udpHeader_t header ;
      header.type_ = header.release ;
      header.length_ = 0 ;
      int numSent = sendto( udpSock_, &header, sizeof( header ), 0, 
                            (struct sockaddr *)&remote_, sizeof( remote_ ) );
      if( sizeof( header ) != numSent )
         perror( "release sendto" );
      prevTime_ = tickMs();
   }
}

class udpGPIO_t : public gpioPoll_t {
public:
   udpGPIO_t( pollHandlerSet_t   &set,
              char const         *devName,
              int                 udpSock,
              sockaddr_in const  &remote,
              unsigned char       mask,
              uiState_t          &ui,
              uiState_t::input_e  uiBit,
              bool                inverted )
      : gpioPoll_t( set, devName )
      , udpSock_( udpSock )
      , remote_( remote )
      , mask_( mask )
      , ui_( ui )
      , uiBit_( uiBit )
      , inverted_( inverted )
   {
   }

   virtual void onHigh( void );
   virtual void onLow( void );

   void sendPinState( void );

private:
   int                 udpSock_ ;
   sockaddr_in         remote_ ;
   unsigned char const mask_ ;
   uiState_t          &ui_ ;
   uiState_t::input_e  uiBit_ ;
   bool const          inverted_ ;
};


void udpGPIO_t :: sendPinState( void )
{
   char data[sizeof(udpHeader_t)+sizeof(gpioPins_t)];
   udpHeader_t &header = *( udpHeader_t * )data ;
   header.type_ = header.pinChange ;
   header.length_ = sizeof( gpioPins_ );
   memcpy( data + sizeof( header ), (void const *)&gpioPins_, sizeof( gpioPins_ ) );

   int numSent = sendto( udpSock_, data, sizeof( data ), 0, 
                         (struct sockaddr *)&remote_, sizeof( remote_ ) );
   if( sizeof( data ) != numSent )
      perror( "pinState sendto" );
}

void udpGPIO_t :: onHigh( void )
{
   if( !inverted_ )
      gpioPins_.pinState_ |= mask_ ;
   else
      gpioPins_.pinState_ &= ~mask_ ;
   sendPinState();
   ui_.setState( uiBit_, true );
}


void udpGPIO_t :: onLow( void )
{
   if( !inverted_ )
      gpioPins_.pinState_ &= ~mask_ ;
   else
      gpioPins_.pinState_ |= mask_ ;

   sendPinState();
   ui_.setState( uiBit_, false );
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


class udpRxPoll_t : public pollHandler_t {
public:
   udpRxPoll_t( int               fdUDP,
                pollHandlerSet_t &set,
                int               fdAudio,
                uiState_t        &ui );

   bool isOpen( void ) const { return 0 <= getFd(); }
   
   //
   // This is where the handling of incoming messages happens.
   //
   virtual void onMsg( deviceMsg_t const & );

   //
   // This is the routine that actually reads and
   // validates the message
   //
   virtual void onDataAvail( void );

protected:
   int const  fdAudio_ ;
   int const  fdLock_ ;
   uiState_t &ui_ ;
};


udpRxPoll_t :: udpRxPoll_t
   ( int               fdUDP,
     pollHandlerSet_t &set,
     int               fdAudio,
     uiState_t        &ui )
      : pollHandler_t( fdUDP, set )
      , fdAudio_( fdAudio )
      , fdLock_( open( "/dev/Turnstile", O_WRONLY ) )
      , ui_( ui )
{
   setMask( POLLIN );
   set.add( *this );
}

void udpRxPoll_t :: onMsg( deviceMsg_t const &msg )
{
   if( deviceMsg_t :: audio_e == msg.type_ )
   {
      playingSomething = true ;
      int numWritten = write( fdAudio_, msg.data_, msg.length_ );
      if( numWritten == msg.length_ )
      {
//         printf( "%lu bytes of audio\n", msg.length_ );
      }
      else
         fprintf( stderr, "Error %m sending audio output\n" );
   }
   else if( deviceMsg_t :: unlock_e == msg.type_ )
   {
      ui_.setState( uiState_t::lock_e, false );
      if( -1 != fdLock_ )
      {
         char const cOut = '\1' ;
         write( fdLock_, &cOut, 1 );
      }
      else
         perror( "/dev/Turnstile" );
   }
   else if( deviceMsg_t :: lock_e == msg.type_ )
   {
      ui_.setState( uiState_t::lock_e, true );
      if( -1 != fdLock_ )
      {
         char const cOut = '\0' ;
         write( fdLock_, &cOut, 1 );
      }
      else
         perror( "/dev/Turnstile" );
   }
   else
      printf( "unknown msgtype %d\n", msg.type_ );
}

void udpRxPoll_t :: onDataAvail( void )
{
   sockaddr_in fromAddr ;
   deviceMsg_t msg ;
   socklen_t   fromSize = sizeof( fromAddr );
   int numRead = recvfrom( getFd(), 
                           (char *)&msg, sizeof( msg ), 0,
                           (struct sockaddr *)&fromAddr, 
                           &fromSize );
   if( offsetof( deviceMsg_t, data_ ) <= numRead )
   {
      // check message lengths
      if( deviceMsg_t :: audio_e == msg.type_ )
      {
         unsigned const expected = sizeof( msg )-sizeof( msg.data_ )+msg.length_ ;
         if( expected != numRead )
         {
            fprintf( stderr, "Weird size : len %lu, expected %lu, read %u\n", msg.length_, expected, numRead );
            return ;
         }
      }
      else if( deviceMsg_t :: unlock_e == msg.type_ )
      {
      }
      else if( deviceMsg_t :: lock_e == msg.type_ )
      {
      }
      else
      {
         printf( "unknown msgtype %d\n", msg.type_ );
         return ;
      }
      
      onMsg( msg );
   }
   else
   {
      fprintf( stderr, "udpRecvfrom:%d:%d:%m\n", numRead, errno );
   }
}

struct pinData_t {
   char const     *name_ ;
   unsigned short  mask_ ;
   bool            inverted_ ;
};

static pinData_t const pins_[] = {
   { "/dev/Feedback",  gpioPins_t::gateOpen_e, false },
   { "/dev/Feedback2", gpioPins_t::carDetect_e, false },
   { "/dev/Feedback3", gpioPins_t::guardShack_e, true },
};

static uiState_t::input_e const pinUI_[] = {
   uiState_t::gateOpen_e,
   (uiState_t::input_e)0,
   (uiState_t::input_e)0
};

static unsigned const pinCount_ = sizeof( pins_ )/sizeof( pins_[0] );

static void *pollThread( void *arg )
{
   threadParam_t const &params = *( threadParam_t const *)arg ;
   
   pollHandlerSet_t handlers ;
   udpBarcode_t     bcPoll( handlers, params.udpSock_, params.remote_, params.mediaFd_ );
   if( bcPoll.isOpen() )
   {
      printf( "opened bcPoll: fd %d, mask %x\n", bcPoll.getFd(), bcPoll.getMask() );

      udpTouch_t touchPoll( handlers, params.udpSock_, params.remote_, params.mediaFd_ );
      if( touchPoll.isOpen() )
      {
         printf( "opened touchPoll: fd %d, mask %x\n", touchPoll.getFd(), touchPoll.getMask() );
         
         uiState_t  uiState ;
         udpGPIO_t *pollPins[pinCount_] = {
            0, 0
         };

         for( unsigned p = 0 ; p < pinCount_ ; p++ )
         {
            char const *devName = pins_[p].name_ ;
            pollPins[p] = new udpGPIO_t( handlers, devName, params.udpSock_, params.remote_, pins_[p].mask_, uiState, pinUI_[p], pins_[p].inverted_ );
            if( pollPins[p]->isOpen() )
               printf( "opened %s: fd %d, mask %x\n", devName, pollPins[p]->getFd(), pollPins[p]->getMask() );
         }
         udpRxPoll_t udpPoll( params.udpSock_,
                              handlers,
                              params.mediaFd_,
                              uiState );
         int iterations = 0 ;
         while( 1 )
         {
            int const ms = ( bcPoll.haveTerminator() || !bcPoll.havePartial() ) 
                           ? -1 
                           : 10 ;
            if( !handlers.poll( ms ) )
            {
               bcPoll.timeout();
            }
         }
      }
      else
         perror( "/dev/touchscreen/ucb1x00" );
   }
   else
      perror( "/dev/ttyS2" );
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
               
                  int not = 0 ;
                  if( 0 != ioctl( fdAudio, SNDCTL_DSP_STEREO, &not ) )
                     perror( "STEREO" );

                  int const vol = 0x6060 ;
                  if( 0 > ioctl( fdAudio, SOUND_MIXER_WRITE_VOLUME, &vol)) 
                     perror( "Error setting volume" );

                  if( 0 < speed )
                  {
                     close( fdAudio );
                     fdAudio = open( "/dev/dsp", O_RDONLY );
                     int fdAudioWrite = open( "/dev/dsp", O_WRONLY );

                     if( 0 > ioctl( fdAudioWrite, SOUND_MIXER_WRITE_VOLUME, &vol)) 
                        perror( "Error setting volume" );
                     
                     if( 0 != ioctl( fdAudioWrite, SNDCTL_DSP_SETFMT, &format) ) 
                        fprintf( stderr, "DSP_SETFMT:%m\n" );

                     if( 0 != ioctl( fdAudioWrite, SNDCTL_DSP_CHANNELS, &channels ) )
                        fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS)\n" );
         
                     if( 0 != ioctl( fdAudioWrite, SNDCTL_DSP_SPEED, &speed ) )
                        fprintf( stderr, "set speed %m\n" );
                  
                     if( 0 != ioctl( fdAudioWrite, SNDCTL_DSP_STEREO, &not ) )
                        perror( "STEREO" );

                     int fdCamera = open( "/dev/video0", O_RDONLY );
//                     if( 0 <= fdCamera )
                     {
                        int const udpSock = socket( AF_INET, SOCK_DGRAM, 0 );
                        if( 0 <= udpSock )
                        {
                           sockaddr_in local ;
                           local.sin_family      = AF_INET ;
                           local.sin_addr.s_addr = INADDR_ANY ;
                           local.sin_port        = htons(0x2020) ;
                           bind( udpSock, (struct sockaddr *)&local, sizeof( local ) );

                           threadParam_t audioParams ;
                           audioParams.remote_ = remote ;
                           audioParams.mediaFd_ = fdAudio ;
                           audioParams.udpSock_ = udpSock ;
                           pthread_t audioHandle ;
                           int create ;
                           create = pthread_create( &audioHandle, 0, audioThread, &audioParams );
                           if( 0 == create )
                           {
                              threadParam_t audioOutParams = audioParams ;
                              audioOutParams.mediaFd_ = fdAudioWrite ;
//                              pthread_t udpHandle ;
//                              create = pthread_create( &udpHandle, 0, udpRxThread, &audioOutParams );
//                              if( 0 == create )
//                              {
                                 pthread_t pollHandle ;
                                 create = pthread_create( &pollHandle, 0, pollThread, &audioOutParams );
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
                                    
                                    pthread_cancel( pollHandle );
                                    void *exitStat ;
                                    pthread_join( pollHandle, &exitStat );
                                 }
                                 else
                                    perror( "pollThreadCreate" );
                              
/*
                                 pthread_cancel( udpHandle );
                                 void *exitStat ;
                                 pthread_join( udpHandle, &exitStat );
                              }
                              else
                              {
                                 printf( "error %mstarting udp thread\n" );
                                 pause();
                              }
*/
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
