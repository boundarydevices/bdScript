#define HAVE_LRINTF
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/dsputil.h"
};

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

static char const * const codecIds[] =  {
    "CODEC_ID_NONE", 
    "CODEC_ID_MPEG1VIDEO",
    "CODEC_ID_H263",
    "CODEC_ID_RV10",
    "CODEC_ID_MP2",
    "CODEC_ID_MP3LAME",
    "CODEC_ID_VORBIS",
    "CODEC_ID_AC3",
    "CODEC_ID_MJPEG",
    "CODEC_ID_MJPEGB",
    "CODEC_ID_MPEG4",
    "CODEC_ID_RAWVIDEO",
    "CODEC_ID_MSMPEG4V1",
    "CODEC_ID_MSMPEG4V2",
    "CODEC_ID_MSMPEG4V3",
    "CODEC_ID_WMV1",
    "CODEC_ID_WMV2",
    "CODEC_ID_H263P",
    "CODEC_ID_H263I",
    "CODEC_ID_SVQ1",
    "CODEC_ID_DVVIDEO",
    "CODEC_ID_DVAUDIO",
    "CODEC_ID_WMAV1",
    "CODEC_ID_WMAV2",
    "CODEC_ID_MACE3",
    "CODEC_ID_MACE6",
    "CODEC_ID_HUFFYUV",
    "CODEC_ID_PCM_S16LE",
    "CODEC_ID_PCM_S16BE",
    "CODEC_ID_PCM_U16LE",
    "CODEC_ID_PCM_U16BE",
    "CODEC_ID_PCM_S8",
    "CODEC_ID_PCM_U8",
    "CODEC_ID_PCM_MULAW",
    "CODEC_ID_PCM_ALAW",
    "CODEC_ID_ADPCM_IMA_QT",
    "CODEC_ID_ADPCM_IMA_WAV",
    "CODEC_ID_ADPCM_MS"
};

static unsigned short numCodecIds = sizeof( codecIds )/sizeof( codecIds[0] );

static char const * const codecTypes[] = {
   "video",
   "audio"
};

void main( int argc, char const * const argv[] )
{
   if( 2 == argc )
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
         if( 0 != ic_ptr->iformat )
            printf( "---> file format %s\n", ic_ptr->iformat->name );
         else
            printf( "??? no file format\n" );
         result = av_find_stream_info( ic_ptr );
         if( 0 <= result )
         {
            printf( "%d stream(s) in file\n", ic_ptr->nb_streams );
            dump_format( ic_ptr, 0, argv[1], false );
            for( int i = 0 ; i < ic_ptr->nb_streams ; i++ )
            {
               AVStream *const str = ic_ptr->streams[i];
               printf( "stream %d -> codec %d (%s), type %s\n", 
                       i, str->codec.codec_id,
                       str->codec.codec_id < numCodecIds 
                           ? codecIds[str->codec.codec_id] 
                           : "unknown",
                       codecTypes[str->codec.codec_type] );
            }
         }
         else
            fprintf( stderr, "Error %d reading stream info\n", result );
         av_close_input_file( ic_ptr );
      }
      else
         fprintf( stderr, "Error opening file %s\n", argv[1] );
   }
   else
      fprintf( stderr, "Usage: testffFormat fileName\n" );
   return 0 ;
}

