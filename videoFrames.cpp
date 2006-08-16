/*
 * Module videoFrames.cpp
 *
 * This module defines the methods of the videoFrames_t
 * class as declared in videoFrames.h
 *
 *
 * Change History : 
 *
 * $Log: videoFrames.cpp,v $
 * Revision 1.4  2006-08-16 02:33:14  ericn
 * -match new mpegDecoder interface
 *
 * Revision 1.3  2005/11/06 00:49:55  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.2  2004/10/30 18:48:52  ericn
 * -use decoder's stride() method
 *
 * Revision 1.1  2003/07/27 15:19:12  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "videoFrames.h"
#include "tickMs.h"
#include <stdio.h>
#include <unistd.h>

videoFrames_t :: videoFrames_t( mpegDemux_t :: streamAndFrames_t const &frames,
                                unsigned maxWidth,
                                unsigned maxHeight )
   : decoder_(),
     frames_( frames ),
     maxWidth_( maxWidth ),
     maxHeight_( maxHeight ),
     queue_( 0 ),
     msStart_( 0LL ),
     lastPTS_( 0 ),
     nextInFrame_( 0 ),
     skippedLast_( false ),
     eof_( false )
{
}

videoFrames_t :: ~videoFrames_t( void )
{
   if( queue_ )
      delete queue_ ;
}

//
// used to pre-buffer a set of frames at startup
//
bool videoFrames_t :: preload( void )
{
   unsigned long frameIdx = 0 ;
   long long lastPTS = 0LL ;

   while( ( frameIdx < frames_.numFrames_ ) 
          &&
          ( ( 0 == queue_ )
            ||
            ( !queue_->isFull() ) ) )
   {
      mpegDemux_t::frame_t const &frame = frames_.frames_[frameIdx++];
      if( 0 != frame.when_ms_ )
         lastPTS = frame.when_ms_ ;

      decoder_.feed( frame.data_, frame.length_, frame.when_ms_ );
      void const *picture ;
      mpegDecoder_t::picType_e type ;
      unsigned temp_ref ;
      long long when ;

      while( decoder_.getPicture( picture, type, temp_ref, when, decoder_.ptAll_e ) )
      {
         if( 0 == queue_ )
         {
            unsigned const width = ( maxWidth_ < decoder_.width() ) ? maxWidth_ : decoder_.width();
            unsigned const height = ( maxHeight_ < decoder_.height() ) ? maxHeight_ : decoder_.height();
            queue_ = new videoQueue_t( width, height );
         }

         videoQueue_t :: entry_t *const entry = queue_->getEmpty();
         entry->when_ms_ = lastPTS ;
         entry->type_    = type ;
         unsigned char *dest = entry->data_ ;
         unsigned char const *src = (unsigned char *)picture ;
         unsigned const imgStride = decoder_.stride();
         for( unsigned i = 0 ; i < queue_->height_ ; i++, dest += queue_->rowStride_, src += imgStride )
         {
            memcpy( dest, src, queue_->rowStride_ );
         }
         queue_->putFull( entry );

         if( queue_->isFull() )
         {
            if( decoder_.getPicture( picture, type, temp_ref, when, decoder_.ptAll_e ) )
               printf( "tail-end picture discarded\n" );
            break;
         }
      }
   }
   
   lastPTS_     = lastPTS ;
   nextInFrame_ = frameIdx ;

   // success only if something was queued
   return 0 != queue_ ;
}

//
// Pull another frame at display time, using 
// idle time to decode additional frame(s).
//
// Returns false at eof
//
bool videoFrames_t :: pull( videoQueue_t :: entry_t *&entry )
{
   if( 0 != queue_ )
   {
      do {
         long long const msNow = tickMs();
         if( msNow >= queue_->frontPTS() + msStart_ )
         {
            entry = queue_->getFull();

            skippedLast_ = false ;
            while( !queue_->isEmpty() )
            {
               long long const nextPTS = queue_->frontPTS() + msStart_ ;
               if( msNow >= nextPTS )
               {
                  skippedLast_ = true ;
                  queue_->putEmpty( entry );
                  entry = queue_->getFull();
               } // skip to next
               else
                  break;
            }

            queue_->putEmpty( entry );
            return true ;
         } // ready to return
         else if( !queue_->isFull() )
         {
            if( nextInFrame_ < frames_.numFrames_ )
            {
               mpegDemux_t::frame_t const &frame = frames_.frames_[nextInFrame_++];
               if( 0 != frame.when_ms_ )
                  lastPTS_ = frame.when_ms_ ;

               decoder_.feed( frame.data_, frame.length_, frame.when_ms_ );
               
               mpegDecoder_t::picType_e which ;
               if( queue_->isEmpty() )
               {
                  printf( "." );
                  which = mpegDecoder_t::ptNoB_e ;
               }
               else
                  which = mpegDecoder_t::ptAll_e ;

               videoQueue_t :: entry_t *entry = queue_->getEmpty();
               void const *picture ;
               void const *lastPicture = 0 ;
               mpegDecoder_t::picType_e type ;
               unsigned temp_ref ;
               long long when ;
               while( decoder_.getPicture( picture, type, temp_ref, when, which ) )
               {
                  lastPicture = picture ;
               }
               
               if( lastPicture )
               {
                  entry->when_ms_ = lastPTS_ ;
                  entry->type_    = type ;
                  unsigned char *dest = entry->data_ ;
                  unsigned char const *src = (unsigned char *)lastPicture ;
                  unsigned const imgStride = decoder_.width()*2 ;
                  for( unsigned i = 0 ; i < queue_->height_ ; i++, dest += queue_->rowStride_, src += imgStride )
                  {
                     memcpy( dest, src, queue_->rowStride_ );
                  }

                  queue_->putFull( entry );
               }
               else
               {
                  queue_->putEmpty( entry );
               }

            } // have more input
            else if( queue_->isEmpty() )
            {
               return false ;
            }
            skippedLast_ = false ;
         } // queue another frame
         else
         {
            skippedLast_ = false ;
            usleep( 10000 ); // wait 1 tick
         }
      } while( 1 );
   } // or why bother ?
   
   
   return true ;
}


