/*
 * Module mpDemux.cpp
 *
 * This module defines the methods of the mpegDemuxr_t 
 * class as declared in mpDemux.h
 *
 *
 * Change History : 
 *
 * $Log: mpDemux.cpp,v $
 * Revision 1.3  2003-07-20 18:43:04  ericn
 * -fixed stand-alone program to match module interface
 *
 * Revision 1.2  2003/07/20 18:36:07  ericn
 * -added PTS interface
 *
 * Revision 1.1  2003/07/20 15:43:16  ericn
 * -Initial version, using libavformat
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "mpDemux.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

mpegDemux_t :: mpegDemux_t
   ( char const *fileName )
   : state_( end_e ),
     avfCtx_( 0 )
{
   avcodec_init();
   mpegps_init();
   mpegts_init();
   raw_init();
   register_protocol(&file_protocol);

   int result = av_open_input_file( &avfCtx_, fileName, NULL, FFM_PACKET_SIZE, NULL );
   if( 0 == result )
   {
      if( 0 != avfCtx_->iformat )
         printf( "---> file format %s\n", avfCtx_->iformat->name );
      else
         printf( "??? no file format\n" );

      memset( &packet_, 0, sizeof( packet_ ) );
      result = av_find_stream_info( avfCtx_ );
      if( 0 <= result )
      {
         printf( "%d stream(s) in file\n", avfCtx_->nb_streams );
         state_ = processing_e ;
      }
      else
         fprintf( stderr, "%s: no stream parser\n", fileName );
   }
   else
      fprintf( stderr, "Error %m reading %s\n", fileName );
}


mpegDemux_t :: ~mpegDemux_t( void )
{
}

inline INT64 pts_to_ms( INT64 pts )
{
   return ( pts / 90 );
}

mpegDemux_t :: frameType_e 
   mpegDemux_t :: getFrame
      ( void const  *&fData,
        unsigned long &length,
        INT64         &when_ms )
{
   fData = 0 ;
   length = 0 ;
   when_ms = -1 ;
   switch( state_ )
   {
      case processing_e :
         {
            if( 0 != packet_.size )
            {
               av_free_packet( &packet_ );
               packet_.size = 0 ;
            }

            while( 1 )
            {
               int result = av_read_packet( avfCtx_, &packet_ );
               if( 0 <= result )
               {
                  when_ms = pts_to_ms( packet_.pts );
                  int const codecType = avfCtx_->streams[packet_.stream_index]->codec.codec_type;
                  if( CODEC_TYPE_AUDIO == codecType )
                  {
                     fData  = packet_.data ;
                     length = packet_.size ;
                     return audioFrame_e ;
                  }
                  else if( CODEC_TYPE_VIDEO == codecType )
                  {
                     fData  = packet_.data ;
                     length = packet_.size ;
                     return videoFrame_e ;
                  } // video
               }
               else 
               {
                  printf( "eof %d\n", result );
                  state_ = end_e ;
                  return endOfFile_e ;
               }
            }
         }
      case end_e :
         return endOfFile_e ;
   }

}

// #define STANDALONE
#ifdef STANDALONE

static char const * const frameTypes_[] = {
   "audioFrame",
   "videoFrame",
   "endOfFile"
};

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <zlib.h>

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      mpegDemux_t decoder( argv[1] );
      mpegDemux_t :: frameType_e ft ;
      void const     *data0 ;
      unsigned long   length ;
      unsigned long   numPackets[2] = { 0, 0 };
      unsigned long   numBytes[2] = { 0, 0 };
      unsigned long   crc[2] = { 0, 0 };
      unsigned long   outBytes[2] = { 0, 0 };
      unsigned long   outCRC[2] = { 0, 0 };

      INT64 when ;

      while( decoder.endOfFile_e != ( ft = decoder.getFrame( data0, length, when ) ) )
      {
         numPackets[ft]++ ;
         numBytes[ft] += length ;
         crc[ft] = adler32( crc[ft], (unsigned char const *)data0, length );
         printf( "%6llu: %s/%p/%lu\n", when, ( ft <= 4 ) ? frameTypes_[ft] : "invalid", data0, length );
      }

      printf( "%4lu audio packets, %7lu bytes, crc 0x%08lx\n", numPackets[0], numBytes[0], crc[0] );
      printf( "%4lu video packets, %7lu bytes, crc 0x%08lx\n", numPackets[1], numBytes[1], crc[1] );
   }
   else
      fprintf( stderr, "Usage: mpDemux fileName\n" );

   return 0 ;
}

#endif
