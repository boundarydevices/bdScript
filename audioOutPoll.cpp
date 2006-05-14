/*
 * Module audioOutPoll.cpp
 *
 * This module defines the methods of the audioOutPoll_t
 * class as declared in audioOutPoll.h
 *
 *
 * Change History : 
 *
 * $Log: audioOutPoll.cpp,v $
 * Revision 1.1  2006-05-14 14:31:35  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "audioOutPoll.h"
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include "audioQueue.h"
#include <sys/ioctl.h>
#include <sys/soundcard.h>

static audioOutPoll_t *instance = 0 ;

audioOutPoll_t &audioOutPoll_t::get( pollHandlerSet_t &set )
{
   if( 0 == instance )
      instance = new audioOutPoll_t(set);
      
   return *instance ;
}

audioOutPoll_t *audioOutPoll_t::get()
{
   return instance ;
}

struct entry_t {
   unsigned short *data_ ;
   unsigned        length_ ;
   unsigned        sampleRate_ ;
   unsigned        numChannels_ ;
   entry_t        *next_ ;
};

static entry_t *head_ = 0 ;
static entry_t *tail_ = 0 ;

void audioOutPoll_t::queuePlayback( audioQueue_t::waveHeader_t &wave )
{
   entry_t *newOne = new entry_t ;

   newOne->data_        = wave.samples_ ;
   newOne->length_      = wave.numSamples_*sizeof(wave.samples_[0]);
   newOne->numChannels_ = wave.numChannels_ ;
   newOne->sampleRate_  = wave.sampleRate_ ;
   newOne->next_ = 0 ;

   if( tail_ )
      tail_->next_ = newOne ;
   else
      head_ = newOne ;
   tail_ = newOne ;
      
   setMask( POLLOUT );
}

static void removeHead()
{
   entry_t *prev = head_ ;
   if( prev )
   {
      head_ = prev->next_ ;
      if( 0 == head_ )
         tail_ = 0 ;
      delete prev ;
   }
}

void audioOutPoll_t::onWriteSpace( void )
{
   gettimeofday( &lastPlayStart_, 0 );
   do {
      if( head_ ) {
         int numWritten = write( getFd(), head_->data_, head_->length_ );
         if( 0 < numWritten )
         {
            head_->length_ -= numWritten ;
            head_->data_   += numWritten ;
            if( 0 == head_->length_ ){
               removeHead();
            ioctl( getFd(), SNDCTL_DSP_POST, 0 );
gettimeofday( &lastPlayEnd_, 0 );
            }
         }
         else
            return ; // output full
      }
   } while( head_ );

   // No more data
   setMask( 0 );
}

audioOutPoll_t:: audioOutPoll_t
   ( pollHandlerSet_t &set,
     char const       *devName )
   : pollHandler_t( openWriteFd(), set )
{
   setMask( POLLOUT );
   set.add( *this );
}

audioOutPoll_t::~audioOutPoll_t( void )
{
}

