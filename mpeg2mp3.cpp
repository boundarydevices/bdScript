#define HAVE_LRINTF
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/dsputil.h"
#include "mpeg2dec/mpeg2.h"
#include "mpeg2dec/video_out.h"
#include "mpeg2dec/convert.h"
};

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/soundcard.h>
#include <linux/input.h>
#include <poll.h>
#include "mad.h"
#include "id3tag.h"
#include "semClasses.h"
#include "fbDev.h"
#include "mtQueue.h"

#include <signal.h>
#include <execinfo.h>
#include <map>

void main( int argc, char const * const argv[] )
{
   if( 3 == argc )
   {
      avcodec_init();

      mpegps_init();
      mpegts_init();
      raw_init();

      register_protocol(&file_protocol);

      AVFormatContext *ic_ptr ;

      int result = av_open_input_file( &ic_ptr, argv[1],
                                       NULL,FFM_PACKET_SIZE, NULL);
      if( 0 == result )
      {
         FILE *fOut = fopen( argv[2], "wb" );
         if( fOut )
         {
            if( 0 != ic_ptr->iformat )
               printf( "---> file format %s\n", ic_ptr->iformat->name );
            else
               printf( "??? no file format\n" );
            result = av_find_stream_info( ic_ptr );
            if( 0 <= result )
            {
               printf( "%d stream(s) in file\n", ic_ptr->nb_streams );
               dump_format( ic_ptr, 0, argv[1], false );
               
               AVPacket pkt;
               while( 1 )
               {
                  int result = av_read_packet( ic_ptr, &pkt );
                  if( 0 <= result )
                  {
                     if( CODEC_TYPE_AUDIO == ic_ptr->streams[pkt.stream_index]->codec.codec_type )
                     {
                        fwrite( pkt.data, pkt.size, 1, fOut );
                     }
                     else if( CODEC_TYPE_VIDEO == ic_ptr->streams[pkt.stream_index]->codec.codec_type )
                     {
                     }
                  }
                  else 
                  {
                     printf( "eof %d\n", result );
                     break;
                  }
               }
            }
            else
               fprintf( stderr, "Error %d reading stream info\n", result );
            
            fclose( fOut );
         }
         else
         {
            perror( argv[2] );
         }
         
         av_close_input_file( ic_ptr );
      }
      else
         fprintf( stderr, "Error opening file %s\n", argv[1] );
   }
   else
      fprintf( stderr, "Usage: testffFormat fileName\n" );
   return 0 ;
}

