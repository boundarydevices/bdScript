/*
 * Module mpegFile.cpp
 *
 * This module defines the methods of the mpegFile_t
 * class as declared in mpegFile.h
 *
 *
 * Change History : 
 *
 * $Log: mpegFile.cpp,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include "mpegFile.h"
#include <sys/errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define DEBUGPRINT
#include "debugPrint.h"

mpegFile_t :: mpegFile_t( char const *filename )
   : fd_( open( filename, O_RDONLY ) )
   , numRead_( 0 )
   , inOffs_( 0 )
   , frameLen_( 0 )
   , framesQueued_( 0 )
   , bytesQueued_( 0 )
   , buffered_( 0 )
   , bufTail_( 0 )
   , free_( 0 )
   , parser_( isOpen() ? new mpegStream_t() : 0 )
{
}

mpegFile_t :: ~mpegFile_t( void )
{
   if( isOpen() )
      close( fd_ );
   if( parser_ )
      delete parser_ ;

   while( buffered_ )
   {
      bufEntry_t *tmp = buffered_ ;
      buffered_ = tmp->next_ ;
      free( tmp );
   }
   
   while( free_ )
   {
      bufEntry_t *tmp = free_ ;
      free_ = tmp->next_ ;
      free( tmp );
   }
}

bool mpegFile_t :: getFrame
   ( unsigned                   whichTypes,    // input: OR of mpegStream_t::frameType_e bits
     mpegStream_t::frameType_e &type,          // output: video or audio
     unsigned char            *&frame,         // output: video or audio data
     unsigned                  &frameLen,      // output: length of frame in bytes
     long long                 &pts,           // output: when to play, ms relative to start
     long long                 &dts,           // output: when to decode, ms relative to start
     unsigned char             &streamId )     // output: which stream if video or audio, frame type if other
{
   if( ( 0 != buffered_ )
       && 
       ( buffered_->type_ & whichTypes ) )
   {
      //
      // move to free chain
      //
      bufEntry_t *tmp = buffered_ ;
      buffered_ = tmp->next_ ;
      if( bufTail_ == tmp )
      {
         bufTail_ = tmp->next_ ; 
         assert( 0 == bufTail_ ); // should be zero
      }
      tmp->next_ = free_ ;
      free_ = tmp ;

      //
      // return data
      //
      type     = tmp->type_ ;
      frame    = tmp->data_ ;
      frameLen = tmp->length_ ;
      pts      = tmp->pts_ ;
      dts      = tmp->dts_ ;
      streamId = tmp->streamId_ ;
      
      --framesQueued_ ;
      bytesQueued_ -= frameLen ;

      return true ;
   } // return buffered data

   while( isOpen() )
   {
      unsigned frameOffs ;
      if( ( numRead_ > inOffs_ )
          &&
          parser_->getFrame( inBuf_+inOffs_, 
                             numRead_-inOffs_,
                             type,
                             frameOffs,
                             frameLen,
                             pts, dts,
                             streamId ) )
      {
         unsigned end = inOffs_+frameOffs+frameLen ;
         if( end > numRead_ )
         {
            unsigned left = end-numRead_ ;
            unsigned max = sizeof(inBuf_)-numRead_ ;
            if( max > left )
            {
               int numRead2 = read( fd_, inBuf_+numRead_, left );
               if( numRead2 == left )
               {
                  fprintf( stderr, "tail end %u bytes\n", left );
               }
               else
               {
                  fprintf( stderr, "packet underflow: %d of %u bytes read\n", numRead_, left );
                  break;
               }
            }
            else
            {
               fprintf( stderr, "packet overflow: %lu bytes needed, %lu bytes avail\n", left, max );
               break;
            }
         } // frame spans end of buffer
         
         frame = inBuf_+inOffs_+frameOffs ;
         inOffs_ += frameOffs+frameLen ; // position for next call
         if( type & whichTypes )
         {
            return true ;
         } // user wants this type
         else
         {
            bufferFrame( type, frame, frameLen, pts, dts, streamId );
            continue ;
         } // buffer this frame
      } // queued data parsed

      // 
      // Need to read some data
      //
      if( inOffs_ < numRead_ )
      {
fprintf( stderr, "leftovers...\n" );
         unsigned leftover = numRead_-inOffs_ ;
         memcpy( inBuf_, inBuf_+inOffs_, leftover );
         inOffs_ = leftover ;
      }
      else
         inOffs_ = 0 ;

      numRead_ = read( fd_, inBuf_ + inOffs_, sizeof(inBuf_)/2 );
      if( 0 >= numRead_ )
      {
         fprintf( stderr, "eof\n" );
         break;
      }
   } // until end of data

   if( isOpen() )
   {
      close( fd_ );
      fd_ = -1 ;
   }
   return false ;
}

void mpegFile_t :: bufferFrame
   ( mpegStream_t::frameType_e  type,
     unsigned char             *frame,
     unsigned                   frameLen,
     long long                  pts,
     long long                  dts,
     unsigned char              streamId )
{
   bufEntry_t *head = free_ ;
   bufEntry_t *tail = 0 ;
   while( head )
   {
      if( head->length_ >= frameLen )
      {
         if( tail )
            tail->next_ = head->next_ ;
         else
            free_ = head->next_ ;
         break;
      } // found a suitable frame
      tail = head ;
      head = tail->next_ ;
   }

   if( 0 == head )
      head = (bufEntry_t *)malloc( sizeof( *head ) + frameLen );

   head->next_     = 0 ;
   head->pts_      = pts ;
   head->dts_      = dts ;
   head->type_     = type ;
   head->streamId_ = streamId ;
   head->length_   = frameLen ;
   memcpy( head->data_, frame, frameLen );

   if( bufTail_ )
   {
      bufTail_->next_ = head ;
      bufTail_ = head ;
   }
   else
      buffered_ = bufTail_ = head ;
   ++framesQueued_ ;
   bytesQueued_ += frameLen ;
}


#ifdef __MODULETEST__

static char const *cFrameTypes_[] = {
     "unknown0"
   , "video"         // 1
   , "audio"         // 2
   , "unknown3"      // 3
   , "other"         // 4
};

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      mpegFile_t fIn( argv[1] );
      if( fIn.isOpen() )
      {
         printf( "%s opened\n", argv[1] );
         
         mpegStream_t::frameType_e type ;
         unsigned char            *frame ;
         unsigned                  frameLen ;
         long long                 pts ;
         long long                 dts ;
         unsigned char             streamId ;
         while( fIn.getFrame( mpegStream_t::audioFrame_e|mpegStream_t::videoFrame_e,
                              type, frame, frameLen, pts, dts, streamId ) )
         {
            fprintf( stderr, "%02x:%s:%p:%u:%llu:%llu\n", streamId, cFrameTypes_[type], frame, frameLen, pts, dts );
         }
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s fileName\n", argv[0] );
   
   return 0 ;
}

#endif 
