/*
 * Module madHeaders.cpp
 *
 * This module defines the methods of the madHeaders_t
 * class as declared in madHeaders.h
 *
 *
 * Change History : 
 *
 * $Log: madHeaders.cpp,v $
 * Revision 1.5  2002-11-24 19:06:58  ericn
 * -modified to use milliseconds, not seconds for output length
 *
 * Revision 1.4  2002/11/14 13:12:44  ericn
 * -modified to allow sounds < 1 second
 *
 * Revision 1.3  2002/11/07 02:13:55  ericn
 * -modified to check length, frequency, and channels
 *
 * Revision 1.2  2002/11/05 15:14:04  ericn
 * -changed name of method
 *
 * Revision 1.1  2002/11/05 05:42:20  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "madHeaders.h"
#include "mad.h"
#include <stdio.h>
#include "id3tag.h"

typedef struct stats_t {
  int currentframe;   /* current frame being played */
  int framecount;     /* total frames in the file   */
  int bitrate;        /* average bitrate            */
  int channels;       /* number of channels (1 or 2 */
  int freq;           /* output sampling frequency  */
  mad_timer_t length; /* length in time of the decoded file */
  mad_timer_t pos;    /* poision in time currently being played */
};


madHeaders_t :: madHeaders_t
   ( void const *data,
     unsigned long length )
   : worked_( false ),
     frames_(),
     numMilliseconds_(0),
     playbackRate_(0),
     numChannels_(0)
{
   struct mad_stream stream;
   struct mad_header header; 
   stats_t           stats ;

   /* open input file */
   
   mad_stream_init(&stream);
   
   /* load some portion of the file into a buffer, or map it into memory */
   
   mad_stream_buffer(&stream, (unsigned char const *)data, length);
   
   bool eof = false ;

   memset( &stats, 0, sizeof( stats ) );
   do {
      if( -1 != mad_header_decode( &header, &stream ) )
      {
         if( 0 == stats.currentframe )
         {
           stats.channels = MAD_NCHANNELS(&header);
           stats.freq = header.samplerate;
         }
         else
         {
            if( stats.channels != MAD_NCHANNELS(&header) )
            {
               printf ("warning: number of channels varies within file %u/%u\n", stats.channels, MAD_NCHANNELS( &header ));
               break;
            }
            if( stats.freq != header.samplerate )
            {
               printf( "error: frequency change during decode %u:%u\n", stats.freq, header.samplerate );
               break;
            }
         }
         
         frames_.push_back( stream.buffer - (unsigned char *)data );
         stats.currentframe++ ;
         mad_timer_add( &stats.length, header.duration );
         stats.bitrate += header.bitrate;
      } // frame decoded
      else
      {
         if( MAD_RECOVERABLE( stream.error ) )
         {
            if( MAD_ERROR_LOSTSYNC == stream.error )
            {
               /* ignore LOSTSYNC due to ID3 tags */
               int tagsize = id3_tag_query (stream.this_frame, stream.bufend - stream.this_frame);
               if (tagsize > 0)
               {
                  printf( "ID3 tag of size %d\n", tagsize );
                  mad_stream_skip (&stream, tagsize);
                  continue;
               }
            }
            printf("error decoding header at frame %d: %s\n", 
                   stats.currentframe, 
                   mad_stream_errorstr (&stream));
         }
         else if( MAD_ERROR_BUFLEN == stream.error )
         {
            eof = true ;
         }
         else
         {
            printf( "unrecoverable error decoding header at frame %d: %s\n", 
                    stats.currentframe, 
                    mad_stream_errorstr( &stream ) );
            break ;
         }
      }
   } while( !eof );
   
   /* close input file */
   
   mad_stream_finish(&stream);
   worked_ = eof 
             && 
             ( ( 0 < stats.length.seconds ) 
               ||
               ( 0 < stats.length.fraction ) )
             && 
             ( 0 < stats.freq ) 
             && 
             ( 0 < stats.channels );
   if( worked_ )
   {
      numMilliseconds_ = ( stats.length.seconds * 1000 );
      //
      // fractional part of mad_timer is in units of 1/MAD_TIMER_RESOLUTION
      // but we want milliseconds.
      //
      //    fraction/MAD_TIMER_RESOLUTION = x/1000
      //    x = 1000*fraction/MAD_TIMER_RESOLUTION
      //
      // but 1000*fraction may over-run a long int, so we'll
      // divide both fraction and MAD_TIMER_RESOLUTION by 1024 
      // first.
      //
      //    x = 1000*((fraction/1024)/(MAD_TIMER_RESOLUTION/1024))
      //
      numMilliseconds_ += (1000 * (stats.length.fraction/1024)) / (MAD_TIMER_RESOLUTION/1024);
      playbackRate_ = stats.freq ;
      numChannels_  = stats.channels ;
   }
}

madHeaders_t :: ~madHeaders_t( void )
{
}


#ifdef __STANDALONE__
#include "memFile.h"

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         madHeaders_t headers( fIn.getData(), fIn.getLength() );
         if( headers.worked() )
         {
            printf( "parsed MP3 headers\n" );
            printf( "%lu frames\n", headers.frames().size() );
            printf( "%lu seconds\n", headers.lengthSeconds() );
            printf( "playback rate %lu\n", headers.playbackRate() );
            printf( "%lu channels\n", headers.numChannels() );
         }
         else
            fprintf( stderr, "Error parsing MP3 headers\n" );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: madHeaders fileName.mp3\n" );

   return 0 ;
}
#endif
