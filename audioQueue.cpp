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
 * Revision 1.1  2002-11-07 02:16:33  ericn
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
               
      int const channels = 2 ;
      if( 0 != ioctl( dspFd, SNDCTL_DSP_CHANNELS, &channels ) )
         fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );
   
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
               sleep( headers.lengthSeconds() );
            }
            else
            {
               fprintf( stderr, "Error parsing MP3 headers\n" );
            }

            if( !shutdown_ )
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
