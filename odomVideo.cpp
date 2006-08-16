/*
 * Module odomVideo.cpp
 *
 * This module defines the methods of the odomVideo_t
 * class as declared in odomVideo.h
 *
 *
 * Change History : 
 *
 * $Log: odomVideo.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomVideo.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tickMs.h"

#define INITTING 1
#define PULLING  2 
#define PUTTING  4

unsigned long volatile flags_ = 0 ;

odomVideo_t::odomVideo_t( 
   odomPlaylist_t &playlist,
   char const     *fileName,
   unsigned        outx,
   unsigned        outy,
   unsigned        outw,
   unsigned        outh )
   : fIn_( fileName )
   , playlist_( playlist )
   , decoder_()
   , outQueue_()
   , bi_( 0 )
   , vStream_( 0 )
   , next_( 0 )
   , end_( 0 )
   , outX_( outx )
   , outY_( outy )
   , outW_( outw )
   , outH_( outh )
   , bytesPerPicture_( 0 )
   , start_( 0LL )
{
   if( fIn_.worked() ){
      mpegDemux_t demuxer( fIn_.getData(), fIn_.getLength() );
      bi_ = demuxer.getFrames();
      if( bi_ ){
         int videoIdx = -1 ;

         for( unsigned char sIdx = 0 ; sIdx < bi_->count_ ; sIdx++ )
         {
            mpegDemux_t::streamAndFrames_t const &sAndF = *bi_->streams_[sIdx];
            mpegDemux_t::frameType_e ft = sAndF.sInfo_.type ;
            if( mpegDemux_t::videoFrame_e == ft ){
               videoIdx = sIdx ;
               break ;
            }
         }
         if( 0 <= videoIdx ){
            vStream_ = bi_->streams_[videoIdx];
         }
      }
      else
         fprintf( stderr, "Error parsing MPEG frames" );
   }
   else
      fprintf( stderr, "%s: %m", fileName );
}

odomVideo_t::~odomVideo_t( void )
{
   if( bi_ ) {
      mpegDemux_t::bulkInfo_t::clear(bi_);
      bi_ = 0 ;
   }
}

bool odomVideo_t::startPlayback( void )
{
   if( !initialized() )
      return false ;
   next_ = vStream_->frames_ ;
   end_  = next_ + vStream_->numFrames_ ;
            
   void const *picture = 0 ;
   unsigned temp_ref ;
   long long when = 0LL ;
   mpegDecoder_t::picType_e type ;
   bool haveHeader = false ;

   while( !haveHeader && ( next_ < end_ ) ){
      decoder_.feed( next_->data_, next_->length_, next_->when_ms_ );
      next_++ ;

      while( decoder_.getPicture( picture, type, temp_ref, when ) ){
         haveHeader = decoder_.haveHeader();
      }
   }
                     
   if( haveHeader ){
      bytesPerPicture_ = decoder_.width() * decoder_.height() * 2;
      if( 0 == outW_ )
         outW_ = decoder_.width();
      if( 0 == outH_ )
         outH_ = decoder_.height();
      int fd = playlist_.fdYUV( decoder_.width(),
                                decoder_.height(),
                                outX_,
                                outY_,
                                outW_,
                                outH_ );
      flags_ |= INITTING ;
      outQueue_.init( decoder_.width(), decoder_.height() );
      flags_ &= ~INITTING ;

      printf( "%u x %u: %u bytes per picture\n"
              "frame rate %u\n", 
              decoder_.width(), decoder_.height(), 
              bytesPerPicture_,
              decoder_.frameRate() );

      start_ = tickMs();
      return (0 <= fd);
   }
   else
      return false ;
}

bool odomVideo_t::playback( void )
{
   while( ( next_ < end_ )
          &&
          !outQueue_.isFull() ){
      void const *picture ;
      mpegDecoder_t::picType_e type ;

      unsigned temp_ref ;
      long long when ;

      if( decoder_.getPicture( picture, type, temp_ref, when, decoder_.ptAll_e ) ){
         if( mpegDecoder_t::ptGOP_e != type ){
            unsigned char *frameMem = outQueue_.pullEmpty();
            if( 0 == frameMem ){
               fprintf( stderr, "Error pulling free frame\n" );
               exit(-1);
            }
            memcpy( frameMem, picture, bytesPerPicture_ );
            outQueue_.putFull( frameMem, when );
         }
      }
      else {
         decoder_.feed( next_->data_, next_->length_, next_->when_ms_ );
         next_++ ;
      }
   }

   if( outQueue_.isFull() ){
      if( !outQueue_.started() )
         outQueue_.start();
   }

   return !completed();
}

bool odomVideo_t::completed( void )
{
   if( ( next_ >= end_ )
       &&
       outQueue_.isEmpty() ){
      printf( "video completed after %lu ms\n"
              "dropped %u frames\n",
              (unsigned long)( tickMs() - start_ ),
              outQueue_.numDropped() );
      return true ;
   }
   else
      return false ;
}

void odomVideo_t::doOutput( void ){
   if( outQueue_.started() && !outQueue_.isEmpty() ){
      long long pts ;

      flags_ |= PULLING ;
      unsigned char *playbackFrame = outQueue_.pull( 0, &pts );
      flags_ &= ~PULLING ;

      if( playbackFrame ){
         int fd = playlist_.fdYUV( decoder_.width(),
                                   decoder_.height(),
                                   outX_,
                                   outY_,
                                   outW_,
                                   outH_ );
         if( 0 <= fd ){
            int const numWritten = write( fd, playbackFrame, bytesPerPicture_ );
            if( (unsigned)numWritten != bytesPerPicture_ )
               fprintf( stderr, "write %d of %lu bytes\n", numWritten, bytesPerPicture_ );
         }
         else
            fprintf( stderr, "Invalid fdYUV\n" );
         
         flags_ |= PUTTING ;
         outQueue_.putEmpty( playbackFrame );
         flags_ |= PUTTING ;
      }
   }
}

void odomVideo_t::dump( void )
{
   printf( "   %lu bytes in file\n", fIn_.getLength() );
   printf( "   next %p, end %p, offset %u, %u left\n", 
           next_, end_, 
           next_ - vStream_->frames_,
           end_-next_ );
   printf( "   w: %u, h: %u\n", decoder_.width(), decoder_.height() );
   printf( "   flags: %lx\n", flags_ );
}

