/*
 * Module audioQueue.cpp
 *
 * This module defines the methods of the audioQueue_t
 * class as declared in audioQueue.h
 *
 *
 * Change History : 
 *
 * $Log: audioQueue.cpp,v $
 * Revision 1.3  2002-11-07 14:39:07  ericn
 * -added audio output, buffering and scheduler spec
 *
 * Revision 1.2  2002/11/07 02:18:13  ericn
 * -fixed shutdown_ flag
 *
 * Revision 1.1  2002/11/07 02:16:33  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "audioQueue.h"
#include "semaphore.h"
#include <unistd.h>
#include "codeQueue.h"
#include "mad.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include "madHeaders.h"

static bool volatile _cancel = false ;
static audioQueue_t *audioQueue_ = 0 ; 

#define OUTBUFSIZE 8192

inline unsigned short scale( mad_fixed_t sample )
{
   return (unsigned short)( sample >> (MAD_F_FRACBITS-15) );
}

void *audioOutputThread( void *arg )
{
   int dspFd = open( "/dev/dsp", O_WRONLY );
   if( 0 <= dspFd )
   {
      if( 0 != ioctl(dspFd, SNDCTL_DSP_SYNC, 0 ) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );
            
      int const format = AFMT_S16_LE ;
      if( 0 != ioctl( dspFd, SNDCTL_DSP_SETFMT, &format) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
               
      unsigned short * const outBuffer = new unsigned short [ OUTBUFSIZE ];
      unsigned short *       nextOut = outBuffer ;
      unsigned short         spaceLeft = OUTBUFSIZE ;

      audioQueue_t *queue = (audioQueue_t *)arg ;
      while( !queue->shutdown_ )
      {
         audioQueue_t :: item_t item ;
         if( queue->pull( item ) )
         {
            madHeaders_t headers( item.data_, item.length_ );
            if( headers.worked() )
            {
               printf( "playback %lu bytes (%lu seconds) from %p here\n", 
                       item.length_, 
                       headers.lengthSeconds(),
                       item.data_ );

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
               do {
                  if( -1 != mad_frame_decode(&frame, &stream ) )
                  {
                     if( 0 == frameId++ )
                     {
                        int sampleRate = frame.header.samplerate ;
                        if( 0 != ioctl( dspFd, SNDCTL_DSP_SPEED, &sampleRate ) )
                        {
                           fprintf( stderr, "Error setting sampling rate to %d:%m\n", sampleRate );
                           break;
                        }
                        
                        int channels = nChannels = MAD_NCHANNELS(&frame.header) ;
                        if( 0 != ioctl( dspFd, SNDCTL_DSP_CHANNELS, &channels ) )
                        {
                           fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );
                           break;
                        }
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
                           *nextOut++ = scale( *left++ );
                           if( 0 == --spaceLeft )
                           {
                              write( dspFd, outBuffer, OUTBUFSIZE*sizeof(outBuffer[0]) );
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
                              write( dspFd, outBuffer, OUTBUFSIZE*sizeof(outBuffer[0]) );
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
               } while( !( eof || _cancel || queue->shutdown_ ) );
   
               if( eof )
               {
                  if( OUTBUFSIZE != spaceLeft )
                  {
                     write( dspFd, outBuffer, (OUTBUFSIZE-spaceLeft)*sizeof(outBuffer[0]) );
                     spaceLeft = OUTBUFSIZE ;
                     nextOut = outBuffer ;
                  } // flush tail end of output
               }
               /* close input file */
               
               mad_stream_finish(&stream);
            }
            else
            {
               fprintf( stderr, "Error parsing MP3 headers\n" );
            }

            if( !queue->shutdown_ )
            {
               if( _cancel )
               {
                  _cancel = false ;
                  queueSource( item.obj_, item.onCancel_, "audioComplete" );
               }
               else
                  queueSource( item.obj_, item.onComplete_, "audioComplete" );
            }
         }
      }
      
      close( dspFd );

      delete [] outBuffer ;

   }
   else
      fprintf( stderr, "Error %m opening audio device\n" );
}

audioQueue_t :: audioQueue_t( void )
   : queue_(),
     threadHandle_( (void *)-1 ),
     shutdown_( false )
{
   assert( 0 == audioQueue_ );
   audioQueue_ = this ;
   pthread_t tHandle ;
   
   int const create = pthread_create( &tHandle, 0, audioOutputThread, this );
   if( 0 == create )
   {
      struct sched_param tsParam ;
      tsParam.__sched_priority = 90 ;
      pthread_setschedparam( tHandle, SCHED_FIFO, &tsParam );
      threadHandle_ = (void *)tHandle ;
      audioQueue_ = this ;
   }
   else
      fprintf( stderr, "Error %m creating curl-reader thread\n" );
}

audioQueue_t :: ~audioQueue_t( void )
{
}

bool audioQueue_t :: insert
   ( JSObject            *mp3Obj,
     unsigned char const *data,
     unsigned             length,
     std::string const   &onComplete,
     std::string const   &onCancel )
{
   item_t item ;
   item.obj_        = mp3Obj ;
   item.data_       = data ;
   item.length_     = length ;
   item.onComplete_ = onComplete ;
   item.onCancel_   = onCancel ;
   return queue_.push( item );
}
   
bool audioQueue_t :: clear( unsigned &numCancelled )
{
   //
   // This should only be run in the context of 
   // a lock on Javascript execution (i.e. from 
   // Javascript code), so there's no need to lock
   // execMutex_.
   //
   mutexLock_t lock( queue_.mutex_ );
   while( !queue_.list_.empty() )
   {
      item_t item = queue_.list_.front();
      queue_.list_.pop_front();
      queueSource( item.obj_, item.onCancel_, "audioComplete" );
   }

   //
   // tell player to cancel at next frame
   //
   _cancel = true ;

   return true ;
}

void audioQueue_t :: shutdown( void )
{
   audioQueue_t *const old = audioQueue_ ;
   audioQueue_ = 0 ;

   if( old )
   {
      old->shutdown_ = true ;
      unsigned cancelled ;
      old->clear( cancelled );
      old->queue_.abort();

      void *exitStat ;
      
      if( 0 <= old->threadHandle_ )
      {
         pthread_join( (pthread_t)old->threadHandle_, &exitStat );
         old->threadHandle_ = (void *)-1 ;
      }
      
      delete old ;
   }
}

bool audioQueue_t :: pull( item_t &item )
{
   return queue_.pull( item );
}
   
audioQueue_t &getAudioQueue( void )
{
   if( 0 == audioQueue_ )
      audioQueue_ = new audioQueue_t();
   return *audioQueue_ ;
}
