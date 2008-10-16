/*
 * Module mpegStream.cpp
 *
 * This module defines the methods of the mpegStreamFile_t class
 * as declared in mpegStream.h
 *
 *
 * Change History : 
 *
 * $Log: mpegStream.cpp,v $
 * Revision 1.8  2008-10-16 00:10:57  ericn
 * [mpegStream test prog] Display pts
 *
 * Revision 1.7  2006-09-17 15:54:32  ericn
 * -use unbuffered I/O for stream
 *
 * Revision 1.6  2006/09/10 01:16:06  ericn
 * -trace events
 *
 * Revision 1.5  2006/08/27 19:13:22  ericn
 * -add mpegFileStream_t class, deprecate mpegStream_t
 *
 * Revision 1.4  2006/08/24 23:59:15  ericn
 * -gutted in favor of mpegPS.h/.cpp
 *
 * Revision 1.3  2006/08/16 02:34:03  ericn
 * -produce MD5 of output
 *
 * Revision 1.2  2006/07/30 21:37:00  ericn
 * -compile under latest libmpeg2
 *
 * Revision 1.1  2005/04/24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include "mpegStream.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

//#define LOGTRACES
#include "logTraces.h"

static traceSource_t traceStreamRead( "fread" );

mpegStreamFile_t::mpegStreamFile_t( int fdIn )
   : fdIn_( fdIn )
   , offset_( 0 )
   , numLeft_( 0 )
   , fileOffs_( 0 )
   , eof_( false )
{
}

mpegStreamFile_t::~mpegStreamFile_t( void )
{
}

bool mpegStreamFile_t::getFrame
   ( unsigned char const       *&frameData,      // output: pointer to frame data
     unsigned                   &frameLen,       // output: bytes in frame
     long long                  &pts,            // output: when to play, ms relative to start
     unsigned char              &streamId,       // output: which stream if video or audio, frame type if other
     CodecType                  &codecType,      // output: VIDEO or AUDIO
     CodecID                    &codecId )       // output: See list in mpegPS.h
{
   do { // returns from the middle on frame available or EOF
      while( 0 < numLeft_ ){
         unsigned char const*next = inBuf_+offset_ ;
         unsigned origLeft = numLeft_ ;
         bool haveFrame = mpegps_read_packet( next, origLeft, frameLen, pts, codecType, codecId, streamId );
         unsigned frameOffs = next - (inBuf_+offset_);
         if( haveFrame ){
            unsigned const numUsed = frameOffs + frameLen ;
            if( numUsed > numLeft_ ){
               unsigned const numNeeded = numUsed-numLeft_ ;
assert( offset_+numNeeded <= sizeof(inBuf_) );

               TRACEINCR( traceStreamRead );
               int numRead = read( fdIn_, inBuf_+offset_+numLeft_, numNeeded );
               TRACEDECR( traceStreamRead );
               if( (unsigned)numRead != numNeeded ){
                  perror( "mpegStreamRead2" );
                  return false ;
               } // short read
               numLeft_ += numRead ; // backed out below
            } // read the rest of the packet

            frameData = inBuf_+offset_+frameOffs ;
assert( ( frameData >= inBuf_ ) && ( (frameData+frameLen) < (inBuf_+sizeof(inBuf_)) ) );
            numLeft_ -= numUsed ;
            offset_ += numUsed ;
            return true ;
         }
         else {
            offset_ += frameOffs ;
            numLeft_ -= frameOffs ;
         }
      } // have data... parse it 

      if( numLeft_ ){
         memcpy( inBuf_, inBuf_+offset_, numLeft_ );
         offset_ = numLeft_ ;
      } // keep tail-end data
      else {
         fileOffs_ += offset_ ;
         offset_ = 0 ;
      }

      assert( offset_ < sizeof(inBuf_)/2 );
      // read more data
      TRACEINCR( traceStreamRead );
      int numRead = read( fdIn_, inBuf_+offset_, sizeof(inBuf_)/2 );
      TRACEDECR( traceStreamRead );
      if( 0 < numRead ){
         numLeft_ += numRead ;
      }
      else {
         if( ( -1 == numRead ) && (EINTR == errno) ){
            printf( "EINTR\n" );
            continue ;
         }
         else if( 0 == numRead )
            eof_ = true ;
         return false ;
      }
   } while( 1 );
}

#ifdef MODULETEST
#include <string.h>
#include <openssl/md5.h>
#include <limits.h>
#include <zlib.h>

static char const *cFrameTypes_[] = {
     "other 0"
   , "video"
   , "audio"
   , "other 3"
   , "other 4"
};

static unsigned const numFrameTypes = sizeof(cFrameTypes_)/sizeof(cFrameTypes_[0]);

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      FILE *fIn = fopen(argv[1], "rb" );
      if( fIn )
      {
         printf( "-------> %s\n", argv[1] );
         
         unsigned long byteCounts[2] = {
            0, 0
         };

         unsigned long frameCounts[2] = {
            0, 0
         };

         MD5_CTX md5_ctx[2];
         MD5_Init(md5_ctx);
         MD5_Init(md5_ctx+1);

         long long startPTS = 0 ;
         long long endPTS = 0 ;

         unsigned long adler = 0 ;
         unsigned char const *frame ;
         unsigned             frameLen ;
         
         CodecType      type ;
         CodecID        codecId ;
         long long      pts ;
         unsigned char  streamId ;

         mpegStreamFile_t stream( fileno(fIn) );

         while( stream.getFrame( frame, frameLen, pts, streamId, type, codecId ) )
         {
            adler = adler32( adler, frame, frameLen );
            printf( "%d/%u/0x%016llx/%08lX/%08lx\n", type, frameLen, pts, adler32( 0, frame, frameLen ), adler );
            bool isVideo = ( CODEC_TYPE_VIDEO == type );
            ++frameCounts[isVideo];
            byteCounts[isVideo] += frameLen ;
            MD5_Update(md5_ctx+isVideo, frame, frameLen );
            if( 0LL != pts ){
               if( 0LL == startPTS )
                  startPTS = pts ;
               endPTS = pts ;
            }
         }
         unsigned char md5sum[MD5_DIGEST_LENGTH];
         MD5_Final( md5sum, md5_ctx);
         printf( "%lu bytes of audio in %lu frames\n", byteCounts[0], frameCounts[0] );
         printf( "   md5 == " );
         for( unsigned i = 0 ; i < MD5_DIGEST_LENGTH ; i++ )
            printf( "%02x ", md5sum[i] );
         printf( "\n" );

         MD5_Final( md5sum, md5_ctx+1);
         printf( "%lu bytes of video in %lu frames\n", byteCounts[1], frameCounts[1] );
         printf( "   md5 == " );
         for( unsigned i = 0 ; i < MD5_DIGEST_LENGTH ; i++ )
            printf( "%02x ", md5sum[i] );
         printf( "\n" );

         long durationPTS = (long)(endPTS-startPTS);
         long durationMs = durationPTS/90 ;
         printf( "pts range: %llu/%llu - %ld (%ld.%lu seconds)\n", startPTS, endPTS, durationPTS, 
                 durationMs/1000, 
                 (unsigned long)(durationMs%1000));

         fclose( fIn );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s fileName\n", argv[0] );

   return 0 ;
}

#endif 
