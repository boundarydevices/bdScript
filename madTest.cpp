/*
 * Program madTest.cpp
 *
 * This program is designed to test multi-file playback
 * of MP3 files.
 *
 * There seems to be a problem on the PXA-250 which causes
 * playback of two short sounds immediately after one another
 * to give weird results. 
 *
 * I haven't yet been able to hear exactly what's going on,
 * only that it's not right.
 *
 * Change History : 
 *
 * $Log: madTest.cpp,v $
 * Revision 1.3  2002-11-30 00:31:01  ericn
 * -removed curlCache and curlThread modules
 *
 * Revision 1.2  2002/11/24 19:07:34  ericn
 * -modified to use madDecoder_t
 *
 * Revision 1.1  2002/11/21 14:09:52  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "audioQueue.h"
#include "semaphore.h"
#include <unistd.h>
#include "mad.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include "madDecode.h"

struct item_t {
   int                   sampleRate_ ;
   unsigned short const *data_ ;
   unsigned              length_ ; // num samples
   unsigned char         numChannels_ ;
};

typedef       mtQueue_t<item_t> queue_t ;
queue_t       queue_ ;
void         *threadHandle_ ;
bool volatile shutdown_ ;
int           dspFd_ ;

#define OUTBUFSIZE 8192

inline unsigned short scale( mad_fixed_t sample )
{
   return (unsigned short)( sample >> (MAD_F_FRACBITS-15) );
}

void *audioOutputThread( void *arg )
{
   dspFd_ = open( "/dev/dsp", O_WRONLY );
   if( 0 <= dspFd_ )
   {
printf( "opened dsp device\n" );
      if( 0 != ioctl(dspFd_, SNDCTL_DSP_SYNC, 0 ) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );
            
      int const format = AFMT_S16_LE ;
      if( 0 != ioctl( dspFd_, SNDCTL_DSP_SETFMT, &format) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
               
      int const channels = 2 ;
      if( 0 != ioctl( dspFd_, SNDCTL_DSP_CHANNELS, &channels ) )
         fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );

      int lastSampleRate = -1 ;

      unsigned short * const outBuffer = new unsigned short [ OUTBUFSIZE ];
      unsigned short *       nextOut = outBuffer ;
      unsigned short         spaceLeft = OUTBUFSIZE ;

      while( !shutdown_ )
      {
         item_t item ;
         if( queue_.pull( item ) )
         {
            if( 0 != ioctl(dspFd_, SNDCTL_DSP_SYNC, 0 ) ) 
               fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );
            if( item.sampleRate_ != lastSampleRate )
            {   
               lastSampleRate = item.sampleRate_ ;
printf( "changing sample rate\n" );
               if( 0 != ioctl( dspFd_, SNDCTL_DSP_SPEED, &item.sampleRate_ ) )
               {
                  fprintf( stderr, "Error setting sampling rate to %d:%m\n", item.sampleRate_ );
                  break;
               }
            }
            
            if( 1 == item.numChannels_ )
            {
               unsigned short const *left = (unsigned short const *)item.data_ ;
               for( unsigned i = 0 ; i < item.length_ ; i++ )
               {
                  unsigned short const sample = *left++ ;
                  *nextOut++ = sample ;
                  *nextOut++ = sample ;
                  spaceLeft -= 2 ;
                  if( 0 == spaceLeft )
                  {
                     write( dspFd_, outBuffer, OUTBUFSIZE*sizeof(outBuffer[0]) );
                     nextOut = outBuffer ;
                     spaceLeft = OUTBUFSIZE ;
                  }
               }
            } // mono
            else
            {
               unsigned short const *left  = (unsigned short const *)item.data_ ;
               unsigned short const *right = (unsigned short const *)item.data_ + 1 ;
               for( unsigned i = 0 ; i < item.length_ ; i++ )
               {
                  *nextOut++ = *left++ ;
                  *nextOut++ = *right++ ;
                  spaceLeft -= 2 ;
                  if( 0 == spaceLeft )
                  {
                     write( dspFd_, outBuffer, OUTBUFSIZE*sizeof(outBuffer[0]) );
                     nextOut = outBuffer ;
                     spaceLeft = OUTBUFSIZE ;
                  }
               }
            } // stereo

            if( OUTBUFSIZE != spaceLeft )
            {
               write( dspFd_, outBuffer, (OUTBUFSIZE-spaceLeft)*sizeof(outBuffer[0]) );
               spaceLeft = OUTBUFSIZE ;
               nextOut = outBuffer ;
            } // flush tail end of output
         }
      }
      
      int temp = dspFd_ ;
      dspFd_ = -1 ;

      close( temp );

      delete [] outBuffer ;

   }
   else
      fprintf( stderr, "Error %m opening audio device\n" );

   return 0 ;
}


#include <unistd.h>

int main( int argc, char const *const argv[] )
{
   curlCache_t &cache = getCurlCache();

   curlFile_t f1( cache.get( "http://linuxbox/effects/push.mp3" ) );
   curlFile_t f2( cache.get( "http://linuxbox/effects/release.mp3" ) );

   if( f1.isOpen() && f2.isOpen() )
   {
      pthread_t tHandle ;
      
      int const create = pthread_create( &tHandle, 0, audioOutputThread, 0 );
      if( 0 == create )
      {
         struct sched_param tsParam ;
         tsParam.__sched_priority = 1 ;
         pthread_setschedparam( tHandle, SCHED_FIFO, &tsParam );
         threadHandle_ = (void *)tHandle ;

         madDecoder_t decode1( f1.getData(), f1.getSize() );
         if( decode1.worked() )
         {
            madDecoder_t decode2( f2.getData(), f2.getSize() );
            if( decode2.worked() )
            {
               item_t item1 ;
               item1.sampleRate_  = decode1.headers().playbackRate();
               item1.numChannels_ = decode1.headers().numChannels();
               item1.data_        = decode1.getSamples();
               item1.length_      = decode1.numSamples();
               item_t item2 ;
               item2.sampleRate_ = decode2.headers().playbackRate();
               item2.numChannels_ = decode2.headers().numChannels();
               item2.data_        = decode2.getSamples();
               item2.length_      = decode2.numSamples();

               int uS = ( 2 == argc ) ? atoi( argv[1] ) : 20000 ;

               while( 1 )
               {
                  printf( "playback\n" );
                  queue_.push( item1 );
                  usleep( uS );
                  queue_.push( item2 );
                  sleep( 5 );
               }
            }
            else
               fprintf( stderr, "Error decoding file2\n" );
         }
         else
            fprintf( stderr, "Error decoding file1\n" );
      }
      else
         fprintf( stderr, "Error %m creating curl-reader thread\n" );
   }
   else
      fprintf( stderr, "Error opening test files\n" );
   return 0 ;
}
