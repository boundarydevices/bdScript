/*
 * Module avSendTo.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: avSendTo.cpp,v $
 * Revision 1.7  2003-10-05 19:13:20  ericn
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
      guardShack_e   = 1,
      gateOpen_e     = 2,
      carDetect_e    = 4
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
            if( playingSomething )
               memset( audioInBuf, 0, inf.fragsize );
            header.length_ = numRead ;
            header.type_   = header.audio ;
            int numSent = sendto( params.udpSock_, audioInBuf, sizeof(udpHeader_t)+numRead, 0, 
                                  (struct sockaddr *)&params.remote_, sizeof( params.remote_ ) );
            if( numSent == ( sizeof(udpHeader_t)+numRead ) )
            {
               audioBytesOut += numRead ;
//               printf( "audio %u, %u\n", numRead, audioBytesOut );
               if( playingSomething )
               {
                  audio_buf_info ai ;
                  if( 0 == ioctl( fdAudio, SNDCTL_DSP_GETOSPACE, &ai) ) 
                  {
                     if( ai.fragments == ai.fragstotal )
                        playingSomething = false ;
                  }
                  else
                  {
                     perror( "SNDCTL_DSP_GETOSPACE" );
                     break;
                  }
               }
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

class udpBarcode_t : public barcodePoll_t {
public:
   udpBarcode_t( pollHandlerSet_t  &set,
                 int                udpSock,
                 sockaddr_in const &remote )
      : barcodePoll_t( set )
      , udpSock_( udpSock )
      , remote_( remote ){}

   virtual void onBarcode( void );

private:
   int         udpSock_ ;
   sockaddr_in remote_ ;
};

void udpBarcode_t :: onBarcode( void )
{
   printf( "barcode <%s>\n", getBarcode() );

   unsigned const len = strlen( getBarcode() );
   char data[sizeof(udpHeader_t)+sizeof(barcode_)];
   udpHeader_t &header = *(udpHeader_t *)data ;
   header.type_   = header.barcode ;
   header.length_ = len + 1 ; // include trailing NULL
   strcpy( data+sizeof(header), getBarcode() );

   unsigned numToSend = sizeof(udpHeader_t)+len ;
   int numSent = sendto( udpSock_, data, numToSend, 0, 
                         (struct sockaddr *)&remote_, sizeof( remote_ ) );
   if( numSent != numToSend )
      perror( "barcode sendto" );
}

class udpTouch_t : public touchPoll_t {
public:
   udpTouch_t( pollHandlerSet_t  &set,
               int                udpSock,
               sockaddr_in const &remote )
      : touchPoll_t( set )
      , udpSock_( udpSock )
      , remote_( remote )
      , wasTouched_( true )
      , prevTime_( 0LL ){}

   // override this to perform processing of a touch
   virtual void onTouch( int x, int y, unsigned pressure, timeval const &tv );
   // override this to perform processing of a release
   virtual void onRelease( timeval const &tv );

private:
   int         udpSock_ ;
   sockaddr_in remote_ ;
   bool        wasTouched_ ;
   long long   prevTime_ ;
};

void udpTouch_t :: onTouch( int x, int y, unsigned pressure, timeval const &tv )
{
   bool send ;
   if( !wasTouched_ )
   {
      wasTouched_ = true ;
      send = true ; // first press
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
   udpGPIO_t( pollHandlerSet_t  &set,
              char const        *devName,
              int                udpSock,
              sockaddr_in const &remote,
              unsigned char      mask )
      : gpioPoll_t( set, devName )
      , udpSock_( udpSock )
      , remote_( remote )
      , mask_( mask ){}

   virtual void onHigh( void );
   virtual void onLow( void );

   void sendPinState( void );
private:
   int                 udpSock_ ;
   sockaddr_in         remote_ ;
   unsigned char const mask_ ;
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
   if( sizeof( header ) != numSent )
      perror( "pinState sendto" );
}

void udpGPIO_t :: onHigh( void )
{
   gpioPins_.pinState_ |= mask_ ;
   sendPinState();
}


void udpGPIO_t :: onLow( void )
{
   gpioPins_.pinState_ &= ~mask_ ;
   sendPinState();
}


struct pinData_t {
   char const     *name_ ;
   unsigned short  mask_ ;
};

static pinData_t const pins_[] = {
   { "/dev/Feedback",  gpioPins_t::gateOpen_e },
   { "/dev/Feedback2", gpioPins_t::guardShack_e },
};

static unsigned const pinCount_ = sizeof( pins_ )/sizeof( pins_[0] );

static void *pollThread( void *arg )
{
   threadParam_t const &params = *( threadParam_t const *)arg ;
   
   pollHandlerSet_t handlers ;
   udpBarcode_t     bcPoll( handlers, params.udpSock_, params.remote_ );
   if( bcPoll.isOpen() )
   {
      printf( "opened bcPoll: fd %d, mask %x\n", bcPoll.getFd(), bcPoll.getMask() );

      udpTouch_t touchPoll( handlers, params.udpSock_, params.remote_ );
      if( touchPoll.isOpen() )
      {
         printf( "opened touchPoll: fd %d, mask %x\n", touchPoll.getFd(), touchPoll.getMask() );
         
         udpGPIO_t *pollPins[pinCount_] = {
            0, 0
         };

         for( unsigned p = 0 ; p < pinCount_ ; p++ )
         {
            char const *devName = pins_[p].name_ ;
            pollPins[p] = new udpGPIO_t( handlers, devName, params.udpSock_, params.remote_, pins_[p].mask_ );
            if( pollPins[p]->isOpen() )
               printf( "opened %s: fd %d, mask %x\n", devName, pollPins[p]->getFd(), pollPins[p]->getMask() );
         }

         int iterations = 0 ;
         while( 1 )
         {
            handlers.poll( -1 );
            printf( "poll %d\n", ++iterations );
         }
      }
      else
         perror( "/dev/touchscreen/ucb1x00" );
   }
   else
      perror( "/dev/ttyS2" );
}

struct deviceMsg_t {
   enum type_e {
      audio_e = 0
   };

   enum {
      MAXSAMPLES = 0x2000
   };
   type_e         type_ ;
   unsigned long  length_ ; // in bytes
   unsigned short data_[MAXSAMPLES];
};


static void *udpRxThread( void *arg )
{
   threadParam_t const &params = *( threadParam_t const *)arg ;
   int const fdAudio = params.mediaFd_ ;
   while( 1 )
   {
      sockaddr_in fromAddr ;
      deviceMsg_t msg ;
      socklen_t   fromSize = sizeof( fromAddr );
      int numRead = recvfrom( params.udpSock_, 
                              (char *)&msg, sizeof( msg ), 0,
                              (struct sockaddr *)&fromAddr, 
                              &fromSize );
      if( 0 < numRead )
      {
         if( deviceMsg_t :: audio_e == msg.type_ )
         {
            unsigned const expected = sizeof( msg )-sizeof( msg.data_ )+msg.length_ ;
            if( expected == numRead )
            {
               playingSomething = true ;
               int numWritten = write( fdAudio, msg.data_, msg.length_ );
               if( numWritten == msg.length_ )
               {
                  printf( "%lu bytes of audio\n", msg.length_ );
               }
               else
                  fprintf( stderr, "Error %m sending audio output\n" );
            }
            else
               fprintf( stderr, "Weird size : len %lu, expected %lu, read %u\n", msg.length_, expected, numRead );
         }
         else
            printf( "unknown msgtype %d\n", msg.type_ );
      }
      else
      {
         perror( "recvfrom" );
         break;
      }
   }
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

                  if( 0 < speed )
                  {
                     int fdCamera = open( "/dev/video0", O_RDONLY );
//                     if( 0 <= fdCamera )
                     {
                        int const udpSock = socket( AF_INET, SOCK_DGRAM, 0 );
                        if( 0 <= udpSock )
                        {
                           sockaddr_in local ;
                           local.sin_family      = AF_INET ;
                           local.sin_addr.s_addr = INADDR_ANY ;
                           local.sin_port        = 0x2020 ;
                           bind( udpSock, (struct sockaddr *)&local, sizeof( local ) );

                           threadParam_t audioParams ; 
                           audioParams.remote_ = remote ;
                           audioParams.mediaFd_ = fdAudio ;
                           audioParams.udpSock_ = udpSock ;
                           pthread_t audioHandle ;
                           int create = pthread_create( &audioHandle, 0, audioThread, &audioParams );
                           if( 0 == create )
                           {
                              threadParam_t audioOutParams = audioParams ;
                              audioOutParams.mediaFd_ = fdAudio ;
                              pthread_t udpHandle ;
                              create = pthread_create( &udpHandle, 0, udpRxThread, &audioOutParams );
                              if( 0 == create )
                              {
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
                              
                                 pthread_cancel( udpHandle );
                                 void *exitStat ;
                                 pthread_join( udpHandle, &exitStat );
                              }
                              else
                              {
                                 printf( "error %mstarting udp thread\n" );
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
