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
 * Revision 1.16  2003-02-01 18:15:29  ericn
 * -preliminary wave file and record support
 *
 * Revision 1.15  2003/01/12 03:04:26  ericn
 * -put sync back
 *
 * Revision 1.14  2003/01/05 01:46:27  ericn
 * -removed SYNC call on completion
 *
 * Revision 1.13  2002/12/15 00:07:58  ericn
 * -removed debug msgs
 *
 * Revision 1.12  2002/12/02 15:14:35  ericn
 * -modified to use pull instead of twiddlin' internals of queue
 *
 * Revision 1.11  2002/12/01 03:13:52  ericn
 * -modified to root objects through audio queue
 *
 * Revision 1.10  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.9  2002/11/30 00:53:43  ericn
 * -changed name to semClasses.h
 *
 * Revision 1.8  2002/11/24 19:08:19  ericn
 * -fixed output queueing
 *
 * Revision 1.7  2002/11/15 14:40:10  ericn
 * -modified to leave speed alone when possible
 *
 * Revision 1.6  2002/11/14 14:55:48  ericn
 * -fixed mono output
 *
 * Revision 1.5  2002/11/14 14:44:40  ericn
 * -fixed clear() routine, modified to always use stereo
 *
 * Revision 1.4  2002/11/14 13:14:08  ericn
 * -modified to expose dsp file descriptor
 *
 * Revision 1.3  2002/11/07 14:39:07  ericn
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
#include "semClasses.h"
#include <unistd.h>
#include "codeQueue.h"
#include "mad.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include "madHeaders.h"
#include "jsGlobals.h"

static bool volatile _cancel = false ;
static bool volatile _playing = false ;
static bool volatile _recording = false ;
static audioQueue_t *audioQueue_ = 0 ; 

#define OUTBUFSIZE 8192

inline unsigned short scale( mad_fixed_t sample )
{
   return (unsigned short)( sample >> (MAD_F_FRACBITS-15) );
}

int getDspFd( void )
{
   return getAudioQueue().readFd_ ;
}

static void audioHandlerCallback( void *cbParam )
{
   audioQueue_t::item_t *item = (audioQueue_t::item_t *)cbParam ;
   jsval handler = item->isComplete_ ? item->onComplete_ : item->onCancel_ ;

   if( JSVAL_VOID != handler )
   {
      executeCode( item->obj_, handler, "audioQueue" );
   }

   JS_RemoveRoot( execContext_, &item->obj_ );
   JS_RemoveRoot( execContext_, &item->onComplete_ );
   JS_RemoveRoot( execContext_, &item->onCancel_ );

   delete item ;
}

void *audioThread( void *arg )
{
printf( "audioOutThread %p (id %x)\n", &arg, pthread_self() );
   audioQueue_t *queue = (audioQueue_t *)arg ;
   if( 0 <= queue->writeFd_ )
   {
      if( 0 != ioctl(queue->writeFd_, SNDCTL_DSP_SYNC, 0 ) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );
            
      int const format = AFMT_S16_LE ;
      if( 0 != ioctl( queue->writeFd_, SNDCTL_DSP_SETFMT, &format) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
               
      int const channels = 2 ;
      if( 0 != ioctl( queue->writeFd_, SNDCTL_DSP_CHANNELS, &channels ) )
         fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );

      int lastSampleRate = -1 ;
      int lastRecordRate = -1 ;

      unsigned short * const outBuffer = new unsigned short [ OUTBUFSIZE ];
      unsigned short *       nextOut = outBuffer ;
      unsigned short         spaceLeft = OUTBUFSIZE ;

      while( !queue->shutdown_ )
      {
         audioQueue_t :: item_t *item ;
         if( queue->pull( item ) )
         {
            if( audioQueue_t :: mp3Play_ == item->type_ )
            {
               unsigned long numWritten = 0 ;
               _playing = true ;
   
               madHeaders_t headers( item->data_, item->length_ );
               if( headers.worked() )
               {
   /*
                  printf( "playback %lu bytes (%lu seconds) from %p here\n", 
                          item->length_, 
                          headers.lengthSeconds(),
                          item->data_ );
   */
                  struct mad_stream stream;
                  struct mad_frame	frame;
                  struct mad_synth	synth;
                  mad_stream_init(&stream);
                  mad_stream_buffer(&stream, (unsigned char const *)item->data_, item->length_ );
                  mad_frame_init(&frame);
                  mad_synth_init(&synth);
   
                  bool eof = false ;
                  unsigned short nChannels = 0 ;               
   
                  spaceLeft = OUTBUFSIZE ;
                  nextOut = outBuffer ;
   
                  unsigned frameId = 0 ;
                  do {
                     if( -1 != mad_frame_decode(&frame, &stream ) )
                     {
                        if( 0 == frameId++ )
                        {
                           int sampleRate = frame.header.samplerate ;
                           if( sampleRate != lastSampleRate )
                           {   
                              lastSampleRate = sampleRate ;
                              if( 0 != ioctl( queue->writeFd_, SNDCTL_DSP_SPEED, &sampleRate ) )
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
                                 write( queue->writeFd_, outBuffer, OUTBUFSIZE*sizeof(outBuffer[0]) );
                                 numWritten += OUTBUFSIZE ;
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
                                 write( queue->writeFd_, outBuffer, OUTBUFSIZE*sizeof(outBuffer[0]) );
                                 nextOut = outBuffer ;
                                 spaceLeft = OUTBUFSIZE ;
                                 numWritten += OUTBUFSIZE ;
                              }
                           }
                        } // stereo
                     } // frame decoded
                     else
                     {
                        if( MAD_RECOVERABLE( stream.error ) )
                           ;
                        else if( MAD_ERROR_BUFLEN == stream.error )
                        {
                           eof = true ;
                        }
                        else
                           break;
                     }
                  } while( !( eof || _cancel || queue->shutdown_ ) );
      
                  _playing = false ;
   
                  if( eof )
                  {
                     if( OUTBUFSIZE != spaceLeft )
                     {
                        write( queue->writeFd_, outBuffer, (OUTBUFSIZE-spaceLeft)*sizeof(outBuffer[0]) );
                        numWritten += OUTBUFSIZE-spaceLeft ;
                     } // flush tail end of output
   //                  memset( outBuffer, 0, OUTBUFSIZE );
   //                  write( queue->writeFd_, outBuffer, OUTBUFSIZE );
                  }
                  /* close input file */
                  
                  mad_synth_finish( &synth );
                  mad_frame_finish( &frame );
                  mad_stream_finish(&stream);
               }
               else
               {
                  fprintf( stderr, "Error parsing MP3 headers\n" );
               }
   
   //            printf( "wrote %lu samples\n", numWritten );
            }
            else if( audioQueue_t::wavPlay_ == item->type_ )
            {
               printf( "playback stuff here\n" );
               _playing = true ;

               audioQueue_t :: waveHeader_t const &header = *( audioQueue_t :: waveHeader_t const * )item->data_ ;
               bool eof = false ;

               spaceLeft = OUTBUFSIZE ;
               nextOut = outBuffer ;

               if( header.sampleRate_ != lastSampleRate )
               {   
                  lastSampleRate = header.sampleRate_ ;
                  if( 0 != ioctl( queue->writeFd_, SNDCTL_DSP_SPEED, &lastSampleRate ) )
                     fprintf( stderr, "Error setting sampling rate to %d:%m\n", lastSampleRate );
               }

               if( 1 == header.numChannels_ )
               {
                  unsigned short const *nextIn = header.samples_ ;
                  unsigned i ;
                  for( i = 0 ; !( _cancel || queue->shutdown_ ) 
                               && 
                               ( i < header.numSamples_ ) ; i++ )
                  {
                     unsigned short const sample = *nextIn++ ;
                     *nextOut++ = sample ;
                     *nextOut++ = sample ;
                     spaceLeft -= 2 ;
                     if( 0 == spaceLeft )
                     {
                        int const outSize = OUTBUFSIZE*sizeof(outBuffer[0]);
                        int numWritten = write( queue->writeFd_, outBuffer, outSize );
                        if( numWritten == outSize )
                        {
                           numWritten += OUTBUFSIZE ;
                           nextOut = outBuffer ;
                           spaceLeft = OUTBUFSIZE ;
                        }
                        else
                           break ;
                     }
                  }
                  eof = ( i == header.numSamples_ )
                        &&
                        ( !_cancel )
                        &&
                        ( !queue->shutdown_ );
                  
                  if( eof && ( spaceLeft != OUTBUFSIZE ) )
                  {
                     int const outSize = (OUTBUFSIZE-spaceLeft)*sizeof(outBuffer[0]);
                     int const numWritten = write( queue->writeFd_, outBuffer, outSize );
                     eof = ( outSize == numWritten );
                  } // flush tail of buffer
               }
               else
               {
                  int const outSize = header.numSamples_*sizeof(header.samples_[0])*header.numChannels_ ;
                  int const numWritten = write( queue->writeFd_, header.samples_, outSize );
                  eof = ( numWritten == outSize )
                        &&
                        ( !_cancel )
                        &&
                        ( !queue->shutdown_ );
               } // stereo, we can write directly

               _playing = false ;
            }
            else if( audioQueue_t::wavRecord_ == item->type_ )
            {
               audioQueue_t :: waveHeader_t &header = *( audioQueue_t :: waveHeader_t * )item->data_ ;
               _recording = true ;
               if( header.sampleRate_ != lastRecordRate )
               {   
                  lastRecordRate = header.sampleRate_ ;
                  if( 0 != ioctl( queue->readFd_, SNDCTL_DSP_SPEED, &lastRecordRate ) )
                     fprintf( stderr, "Error setting sampling rate to %d:%m\n", lastRecordRate );
               }

               unsigned short *nextSample = header.samples_ ;
               unsigned long bytesLeft = header.numSamples_ * sizeof( header.samples_[0] );
               while( !( _cancel || queue->shutdown_ ) 
                      && 
                      ( 0 < bytesLeft ) )
               {
                  // read max of 1/2 second at a time
                  int const readSize = ( bytesLeft > header.sampleRate_ ) ? header.sampleRate_ : bytesLeft ;
                  int numRead = read( queue->readFd_, nextSample, bytesLeft );
                  if( 0 < numRead )
                  {
                     bytesLeft -= numRead ;
                  }
                  else
                  {
                     perror( "audio read" );
                     break;
                  }
               }

               header.numSamples_ = ( header.numSamples_ - bytesLeft ) / sizeof( header.samples_[0] );
               header.numChannels_ = 1 ;

            }
            else
               fprintf( stderr, "unknown audio queue request %d\n", item->type_ );

            if( !queue->shutdown_ )
            {
               if( _cancel )
               {
                  _cancel = false ;
                  queueCallback( audioHandlerCallback, item );

                  if( 0 != ioctl( queue->writeFd_, SNDCTL_DSP_RESET, 0 ) ) 
                     fprintf( stderr, ":ioctl(SNDCTL_DSP_RESET):%m" );
               }
               else
               {
                  if( 0 != ioctl( queue->writeFd_, SNDCTL_DSP_POST, 0 ) ) 
                     fprintf( stderr, ":ioctl(SNDCTL_DSP_POST):%m" );
                  if( 0 != ioctl( queue->writeFd_, SNDCTL_DSP_SYNC, 0 ) ) 
                     fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );
                  item->isComplete_ = true ;
                  queueCallback( audioHandlerCallback, item );
               }
            }
         }
      }
      
      delete [] outBuffer ;

   }
   else
      fprintf( stderr, "Error %m opening audio device\n" );

   return 0 ;
}

audioQueue_t :: audioQueue_t( void )
   : queue_(),
     threadHandle_( (void *)-1 ),
     shutdown_( false ),
     readFd_( open( "/dev/dsp", O_RDONLY ) ),
     writeFd_( open( "/dev/dsp", O_WRONLY ) )
{
   assert( 0 == audioQueue_ );
   audioQueue_ = this ;
   pthread_t tHandle ;
   
   int recordLevel = 100 ;
   if( 0 == ioctl( readFd_, MIXER_WRITE( SOUND_MIXER_MIC ), &recordLevel ) )
      printf( "record level is now %d\n", recordLevel );
   else
      perror( "set record level" );

   int const create = pthread_create( &tHandle, 0, audioThread, this );
   if( 0 == create )
   {
      struct sched_param tsParam ;
      tsParam.__sched_priority = 90 ;
      pthread_setschedparam( tHandle, SCHED_FIFO, &tsParam );
      threadHandle_ = (void *)tHandle ;
printf( "readFd == %d\n"
        "queue->writeFd_ == %d\n", readFd_, writeFd_ );
   }
   else
      fprintf( stderr, "Error %m creating curl-reader thread\n" );
}

audioQueue_t :: ~audioQueue_t( void )
{
}

bool audioQueue_t :: queuePlayback
   ( JSObject            *mp3Obj,
     unsigned char const *data,
     unsigned             length,
     jsval                onComplete,
     jsval                onCancel )
{
   item_t *item = new item_t ;
   item->type_       = mp3Play_ ;
   item->obj_        = mp3Obj ;
   item->data_       = data ;
   item->length_     = length ;
   item->onComplete_ = onComplete ;
   item->onCancel_   = onCancel ;
   item->isComplete_ = false ;
   JS_AddRoot( execContext_, &item->obj_ );
   JS_AddRoot( execContext_, &item->onComplete_ );
   JS_AddRoot( execContext_, &item->onCancel_ );
   return queue_.push( item );
}
   
bool audioQueue_t :: queuePlayback
   ( JSObject            *obj,
     waveHeader_t const  &data,
     jsval                onComplete,
     jsval                onCancel )
{
   item_t *item = new item_t ;
   item->type_       = wavPlay_ ;
   item->obj_        = obj ;
   item->data_       = (unsigned char const *)&data ;
   item->length_     = 0 ;
   item->onComplete_ = onComplete ;
   item->onCancel_   = onCancel ;
   item->isComplete_ = false ;
   JS_AddRoot( execContext_, &item->obj_ );
   JS_AddRoot( execContext_, &item->onComplete_ );
   JS_AddRoot( execContext_, &item->onCancel_ );
   return queue_.push( item );
}

bool audioQueue_t :: clear( unsigned &numCancelled )
{
   numCancelled = 0 ;

   {
      //
      // This should only be run in the context of 
      // a lock on Javascript execution (i.e. from 
      // Javascript code), so there's no need to lock
      // execMutex_.
      //
      item_t *item ;
      while( queue_.pull( item, 0 ) )
      {
         item->isComplete_ = false ;
         queueCallback( audioHandlerCallback, item );
         ++numCancelled ;
      }
   
      //
      // tell player to cancel at next frame
      //
      if( _playing || _recording )
      {
         _cancel = true ;
         ++numCancelled ;
      }
   } // limit scope of lock
      
   ioctl( readFd_, SNDCTL_DSP_SYNC, 0 );
   ioctl( writeFd_, SNDCTL_DSP_SYNC, 0 );

   return true ;
}

void audioQueue_t :: shutdown( void )
{
   audioQueue_t *const old = audioQueue_ ;
   audioQueue_ = 0 ;

   if( old )
   {
      close( old->readFd_ );
      close( old->writeFd_ );
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

bool audioQueue_t :: pull( item_t *&item )
{
   return queue_.pull( item );
}
   
audioQueue_t &getAudioQueue( void )
{
   if( 0 == audioQueue_ )
      audioQueue_ = new audioQueue_t();
   return *audioQueue_ ;
}
