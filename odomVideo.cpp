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
 * Revision 1.4  2006-09-01 20:24:17  ericn
 * -keep track of PTS stats
 *
 * Revision 1.3  2006/09/01 01:02:27  ericn
 * -use mpegQueue for odomVideo
 *
 * Revision 1.2  2006/08/22 15:50:10  ericn
 * -remove GOP pseudo-pic from mpegDecode
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
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
   odomPlaylist_t    &playlist,
   char const        *fileName,
   rectangle_t const &outRect )
   : fIn_( fopen( fileName, "rb" ) )
   , stream_( fIn_ )
   , outQueue_( -1,
                playlist.fdYUV(),
                1,
                outRect )
   , playlist_( playlist )
   , outRect_( outRect )
   , bytesPerPicture_( 0 )
   , firstPTS_( 0LL )
   , lastPTS_( 0LL )
   , start_( 0LL )
{
   if( 0 == fIn_ )fprintf( stderr, "%s: %m", fileName );
}

odomVideo_t::~odomVideo_t( void )
{
   if( fIn_ )
      fclose( fIn_ );
}

bool odomVideo_t::startPlayback( void )
{
   if( !initialized() )
      return false ;

   bool firstVideo = true ;
   while( outQueue_.msToBuffer() > outQueue_.msVideoQueued() ){
      unsigned char const *frame ;
      unsigned             frameLen ;
      
      CodecType      type ;
      CodecID        codecId ;
      long long      pts ;
      unsigned char  streamId ;
   
      if( stream_.getFrame( frame, frameLen, pts, streamId, type, codecId ) ){
         bool isVideo = ( CODEC_TYPE_VIDEO == type );
         if( isVideo ){
            if( 0LL != pts ){
               if( 0LL == firstPTS_ )
                  firstPTS_ = pts ;
               lastPTS_ = pts ;
            }
            long long ms = outQueue_.ptsToMs(pts);
            outQueue_.feedVideo( frame, frameLen, firstVideo, ms );
            firstVideo = false ;
         }
      }
      else
         return false ;
   }

   start_ = tickMs();
   return true ;
}

bool odomVideo_t::playback( void )
{
   unsigned char const *frame ;
   unsigned             frameLen ;
   
   CodecType      type ;
   CodecID        codecId ;
   long long      pts ;
   unsigned char  streamId ;

   if( stream_.getFrame( frame, frameLen, pts, streamId, type, codecId ) ){
      bool isVideo = ( CODEC_TYPE_VIDEO == type );
      if( isVideo ){
         outQueue_.feedVideo( frame, frameLen, false, outQueue_.ptsToMs(pts) );
      }
   }

   return !completed();
}

bool odomVideo_t::completed( void )
{
   if( ( ( 0 == fIn_ )
         || 
         feof( fIn_ ) )
       &&
       outQueue_.isEmpty() ){
      printf( "video completed after %lu ms\n"
              , (unsigned long)( tickMs() - start_ ) );
      dump();
      
      return true ;
   }
   else
      return false ;
}

void odomVideo_t::doOutput( void ){
   long long const now = tickMs();
   outQueue_.playVideo( now );
}

void odomVideo_t::dump( void )
{
   printf( "%lu ms of video queued\n"
           "   %u buffers allocated\n"
           "   %u buffers freed\n"
              "%u frames queued\n"
              "%u frames played\n"
              "%u frames skipped\n"
              "%u frames dropped\n"
              "PTS range: %lld..%lld (%lu)\n"
         ,  outQueue_.msVideoQueued()
         ,  outQueue_.numAllocated()
         ,  outQueue_.numFreed()
         ,  outQueue_.vFramesQueued()
         ,  outQueue_.vFramesPlayed()
         ,  outQueue_.vFramesSkipped() 
         ,  outQueue_.vFramesDropped()
         ,  firstPTS_
         ,  lastPTS_
         ,  (unsigned long)(lastPTS_-firstPTS_)
         );
}

