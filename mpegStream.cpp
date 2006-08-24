/*
 * Module mpegStream.cpp
 *
 * This module defines the methods of the mpegStream_t class
 * as declared in mpegStream.h
 *
 *
 * Change History : 
 *
 * $Log: mpegStream.cpp,v $
 * Revision 1.4  2006-08-24 23:59:15  ericn
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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "mpegPS.h"

int doTrace = 1 ;

inline void trace( char const *msg, ... )
{
   if( doTrace )
   {
      va_list ap ;
      va_start( ap, msg );

      vfprintf( stderr, msg, ap );
      va_end( ap );
      fflush( stdout );
   }
}

#define MAX_SYNC_SIZE 100000

mpegStream_t :: mpegStream_t( void )
   : numStreams_( 0 )
{
}

mpegStream_t :: ~mpegStream_t( void )
{
}

static mpegStream_t::frameType_e frameConvert_[3] = {
   mpegStream_t::otherFrame_e    // CODEC_TYPE_UNKNOWN = -1,
,  mpegStream_t::videoFrame_e    // CODEC_TYPE_VIDEO,
,  mpegStream_t::audioFrame_e    // CODEC_TYPE_AUDIO,
};

bool mpegStream_t :: getFrame
   ( unsigned char const *fData,          // input: data to parse
     unsigned             length,         // input: bytes available
     frameType_e         &type,           // output: video or audio
     unsigned            &offset,         // output: offset of start of frame
     unsigned            &frameLen,       // output: bytes in frame
     long long           &pts,            // output: when to play, ms relative to start
     long long           &dts,            // output: when to decode, ms relative to start
     unsigned char       &streamId )      // output: which stream (if multiple)
{
   dts = 0LL ;
   unsigned char const *const start = fData ;
     
   CodecType codecType ;
   CodecID   codecId ;

   if( mpegps_read_packet( 
         fData, length, 
         frameLen, pts, 
         codecType, codecId,
         streamId ) )
   {
      type = frameConvert_[codecType+1];
      offset = fData - start ;
      return true ;
   }

   return false ;
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
         mpegStream_t mpeg ;
         
         unsigned long byteCounts[2] = {
            0, 0
         };

         unsigned long frameCounts[2] = {
            0, 0
         };

         MD5_CTX md5_ctx[2];
         MD5_Init(md5_ctx);
         MD5_Init(md5_ctx+1);

         unsigned maxPacket = 0 ;
         unsigned minPacket = UINT_MAX ;

         long long startPTS = 0 ;
         long long endPTS = 0 ;

         unsigned long globalOffs = 0 ;
         unsigned char inBuf[4096];
         unsigned long adler = 0 ;
         int  numRead ;
         while( 0 < ( numRead = fread( inBuf, 1, sizeof(inBuf)/2, fIn ) ) )
         {
            int                  inOffs = 0 ;
            
            mpegStream_t::frameType_e type ;
            unsigned                  frameOffs ;
            unsigned                  frameLen ;
            long long                 pts ;
            long long                 dts ;
            unsigned char             streamId ;

            while( ( numRead > (int)inOffs )
                   &&
                   mpeg.getFrame( inBuf+inOffs, 
                                  numRead-inOffs,
                                  type,
                                  frameOffs,
                                  frameLen,
                                  pts, dts,
                                  streamId ) )
            {
               bool const isVideo = (mpegStream_t::videoFrame_e == type );
               byteCounts[isVideo] += frameLen ;
               frameCounts[isVideo]++ ;

               if( 0 != pts ){
                  if( 0 == startPTS )
                     startPTS = pts ;
                  endPTS = pts ; 
               }

               int end = inOffs+frameOffs+frameLen ;
               if( end > numRead )
               {
                  int left = end-numRead ;
                  int max = sizeof(inBuf)-numRead ;
                  if( max > left )
                  {
                     int numRead2 = fread( inBuf+numRead, 1, left, fIn );
                     if( numRead2 == left )
                     {
                        numRead += numRead2 ;
                     }
                     else
                     {
                        fprintf( stderr, "packet underflow: %d of %u bytes read\n", numRead2, left );
                        return 0 ;
                     }
                  }
                  else
                  {
                     fprintf( stderr, "packet overflow: %lu bytes needed, %lu bytes avail\n", left, max );
                     return 0 ;
                  }
               }

               unsigned char *frameStart = inBuf+inOffs+frameOffs ;
               adler = adler32( adler, frameStart, frameLen );
               printf( "%d/%lu/%u/%08lX/%08lx\n", type, globalOffs+inOffs+frameOffs, frameLen, adler32( 0, frameStart, frameLen ), adler );
               MD5_Update(md5_ctx+isVideo, frameStart, frameLen );

               if( type >= (int)numFrameTypes )
                  fprintf( stderr, "unknown frame type %u\n", type ); 
               
               if( frameLen > maxPacket )
                  maxPacket = frameLen ;
               if( frameLen < minPacket )
                  minPacket = frameLen ;

               inOffs += frameOffs+frameLen ;
            }
            globalOffs += numRead ;
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

         printf( "min/max packet size == %u/%u\n", minPacket, maxPacket );
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
