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
 * Revision 1.1  2002-11-21 14:09:52  ericn
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
#include "madHeaders.h"

struct item_t {
   unsigned char *data_ ;
   unsigned       length_ ;
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
            madHeaders_t headers( item.data_, item.length_ );
            if( headers.worked() )
            {
/*               printf( "playback %lu bytes (%lu seconds) from %p here\n", 
                       item.length_, 
                       headers.lengthSeconds(),
                       item.data_ );
*/
               struct mad_stream stream;
               struct mad_frame	frame;
               struct mad_synth	synth;
               mad_stream_init(&stream);
               mad_stream_buffer(&stream, (unsigned char const *)item.data_, item.length_ );
               mad_frame_init(&frame);
               mad_synth_init(&synth);

               bool eof = false ;
               unsigned short nChannels = 0 ;               
               
               unsigned frameId = 0 ;

               if( 0 != ioctl(dspFd_, SNDCTL_DSP_SYNC, 0 ) ) 
                  fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );

               do {
                  if( -1 != mad_frame_decode(&frame, &stream ) )
                  {
                     if( 0 == frameId++ )
                     {
                        int sampleRate = frame.header.samplerate ;
                        if( sampleRate != lastSampleRate )
                        {   
                           lastSampleRate = sampleRate ;
printf( "changing sample rate\n" );
                           if( 0 != ioctl( dspFd_, SNDCTL_DSP_SPEED, &sampleRate ) )
                           {
                              fprintf( stderr, "Error setting sampling rate to %d:%m\n", sampleRate );
                              break;
                           }
                        }

                        nChannels = MAD_NCHANNELS(&frame.header) ;

                     } // first frame... check sample rate
                     else
                     {
                     } // not first frame
         
                     mad_synth_frame( &synth, &frame );
                     if( 1 == nChannels )
                     {
                        mad_fixed_t const *left = synth.pcm.samples[0];
                        for( unsigned i = 0 ; i < synth.pcm.length ; i++ )
                        {
                           unsigned short const sample = scale( *left++ );
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
                        mad_fixed_t const *left  = synth.pcm.samples[0];
                        mad_fixed_t const *right = synth.pcm.samples[1];
                        for( unsigned i = 0 ; i < synth.pcm.length ; i++ )
                        {
                           *nextOut++ = scale( *left++ );
                           *nextOut++ = scale( *right++ );
                           spaceLeft -= 2 ;
                           if( 0 == spaceLeft )
                           {
                              write( dspFd_, outBuffer, OUTBUFSIZE*sizeof(outBuffer[0]) );
                              nextOut = outBuffer ;
                              spaceLeft = OUTBUFSIZE ;
                           }
                        }
                     } // stereo
                  } // frame decoded
                  else
                  {
                     if( !MAD_RECOVERABLE( stream.error ) )
                        break;
                  }
               } while( !( eof || shutdown_ ) );
   
               if( eof )
               {
                  if( OUTBUFSIZE != spaceLeft )
                  {
                     write( dspFd_, outBuffer, (OUTBUFSIZE-spaceLeft)*sizeof(outBuffer[0]) );
                     spaceLeft = OUTBUFSIZE ;
                     nextOut = outBuffer ;
                  } // flush tail end of output
                  
                  if( 0 != ioctl(dspFd_, SNDCTL_DSP_POST, 0 ) ) 
                     fprintf( stderr, ":ioctl(SNDCTL_DSP_POST):%m" );
               }
               /* close input file */
               
               mad_stream_finish(&stream);
            }
            else
            {
               fprintf( stderr, "Error parsing MP3 headers\n" );
            }
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


#include "curlCache.h"
#include <unistd.h>

int main( void )
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
         item_t item1 ;
         item1.data_    = (unsigned char *)malloc( f1.getSize() );
         memcpy( item1.data_, f1.getData(), f1.getSize() );
         item1.length_  = f1.getSize();
         item_t item2 ;
         item2.data_    = (unsigned char *)malloc( f2.getSize() );
         memcpy( (unsigned char *)item2.data_, f2.getData(), f2.getSize() );
         item2.length_  = f2.getSize();
         while( 1 )
         {
            printf( "playback\n" );
            queue_.push( item1 );
            queue_.push( item2 );
            sleep( 5 );
         }
      }
      else
         fprintf( stderr, "Error %m creating curl-reader thread\n" );
   }
   else
      fprintf( stderr, "Error opening test files\n" );
   return 0 ;
}
