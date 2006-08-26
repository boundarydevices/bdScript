/*
 * Module odomStream.cpp
 *
 * This module defines the methods of the odomVideoStream_t
 * class as declared in odomStream.h
 *
 *
 * Change History : 
 *
 * $Log: odomStream.cpp,v $
 * Revision 1.3  2006-08-26 16:06:18  ericn
 * -use new mpegQueue instead of decoder+odomVQ
 *
 * Revision 1.2  2006/08/22 15:58:22  ericn
 * -match new mpegDecoder interface
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomStream.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "tickMs.h"

odomVideoStream_t::odomVideoStream_t
   ( odomPlaylist_t    &playlist, // used to get video fd
     unsigned           port,
     rectangle_t const &outRect )
   : mpegRxUDP_t( port )
   , playlist_( playlist )
   , outRect_( outRect )
   , outQueue_( open( "/dev/dsp", O_WRONLY ), playlist.fdYUV(), 1, outRect )
   , firstFrame_( true )
{
}

odomVideoStream_t::~odomVideoStream_t( void )
{
}

void odomVideoStream_t::onNewFile( 
   char const *fileName,
   unsigned    fileNameLen )
{
   printf( "new file %s\n", fileName );
   firstFrame_ = true ;
}

void odomVideoStream_t::onRx( 
   bool                 isVideo,
   bool                 discont,
   unsigned char const *fData,
   unsigned             length,
   long long            pts,
   long long            dts )
{
   discont = discont | firstFrame_ ;

   if( isVideo ){
      if( firstFrame_ ){
         outQueue_.adjustPTS( pts );
         firstFrame_ = false ;
         startMs_ = pts ;
      }
      if( 0 != pts )
         lastMs_ = pts ;
      outQueue_.feedVideo( fData, length, discont, pts );
   }
   else
      outQueue_.feedAudio( fData, length, discont, pts );
}

void odomVideoStream_t::onEOF( 
   char const   *fileName,
   unsigned long totalBytes,
   unsigned long videoBytes,
   unsigned long audioBytes )
{
   printf( "eof(%s): %lu bytes (%lu video, %lu audio)\n", fileName, totalBytes, videoBytes, audioBytes );
   printf( "ms: %ld... %llu->%llu\n", (unsigned long)(lastMs_-startMs_), startMs_, lastMs_ );
}

void odomVideoStream_t::doOutput( void )
{
   long long const now = tickMs();
   outQueue_.playVideo( now );
}

void odomVideoStream_t::dump( void )
{
}


