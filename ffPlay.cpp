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
#include <poll.h>
#include "mad.h"
#include "id3tag.h"
#include "semClasses.h"
#include "voQueue.h"
#include "fbDev.h"
#include "mtQueue.h"

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

#define PIC_MASK_CODING_TYPE_I ( 1 << ( PIC_FLAG_CODING_TYPE_I - 1 ) )
#define PIC_MASK_CODING_TYPE_P ( 1 << ( PIC_FLAG_CODING_TYPE_P - 1 ) )
#define PIC_MASK_CODING_TYPE_B ( 1 << ( PIC_FLAG_CODING_TYPE_B - 1 ) )
#define PIC_MASK_CODING_TYPE_D ( 1 << ( PIC_FLAG_CODING_TYPE_D - 1 ) )
#define PIC_MASK_CODING_MASK   0x0F
#define PIC_MASK_ALLTYPES      PIC_MASK_CODING_MASK

static unsigned short numCodecIds = sizeof( codecIds )/sizeof( codecIds[0] );

inline unsigned short scale( mad_fixed_t sample )
{
   return (unsigned short)( sample >> (MAD_F_FRACBITS-15) );
}

static char const * const codecTypes[] = {
   "video",
   "audio"
};

static INT64 now_us()
{
   struct timeval now ; gettimeofday( &now, 0 );
   return ((INT64)now.tv_sec*1000000)+(INT64)now.tv_usec ;
}

static INT64 pts_to_us( INT64 pts, int numerator, int denominator )
{
   return ( ( pts*numerator )*1000000 ) / denominator ;
}

static void us_sleep( INT64 delta_us )
{
   timespec ts ;
   ts.tv_sec = 0 ;
   ts.tv_nsec = delta_us*1000 ;
   nanosleep( &ts, 0 );
}

struct videoFrame_t {
   INT64         when_ ;
   unsigned char data_[1];
};

struct audioFrame_t {
   INT64         when_ ;
   unsigned      length_ ;
   unsigned char data_[4096];
};

class mmQueue_t {
public:
   mmQueue_t( int fdDsp );
   ~mmQueue_t( void );

   //
   // feed-side 
   //
   void          initVideo( unsigned short width, 
                            unsigned short height );
   videoFrame_t *nextVideoFrame( void );
   void          postVideoFrame( void );
   
   audioFrame_t *nextAudioFrame( void );
   void          postAudioFrame( void );
   
   void          shutdown( void );


   //
   // read side
   //
   bool pullAudio( audioFrame_t *&aframe );
   bool pullVideo( videoFrame_t *&vframe );
   bool wait( void );
   void releaseVideoFrame();
   void releaseAudioFrame();

// private:
   void startThread( void );
   pthread_t               tHandle_ ;
   unsigned short          mpgWidth_ ;
   unsigned short          mpgHeight_ ;
   unsigned long           mpgStride_ ;
   unsigned short          width_ ;
   unsigned short          height_ ;
   unsigned long           imgStride_ ;
   unsigned long           fbStride_ ;
   pthread_mutex_t         mutex_ ;
   pthread_cond_t          feedVideoCond_ ;
   pthread_cond_t          feedAudioCond_ ;
   pthread_cond_t          pullCond_ ;
   unsigned                vFrameSize_ ;
   unsigned char          *frameMem_ ;
   videoFrame_t          **videoFrames_ ;
   unsigned short volatile videoFrameAdd_ ;
   unsigned short volatile videoFrameTake_ ;
   unsigned char          *audioMem_ ;
   audioFrame_t          **audioFrames_ ;
   unsigned short volatile audioFrameAdd_ ;
   unsigned short volatile audioFrameTake_ ;
   int                     dspFd_ ;
   unsigned long           ptsNumerator_ ;
   unsigned long           ptsDenominator_ ;
   int                     numAudioChannels_ ;
   int                     lastSampleRate_ ;
   int            volatile displayFrames_ ;
   bool           volatile shutdown_ ;
};

static void getDimensions( AVFormatContext const &ctx,
                           unsigned short        &width,
                           unsigned short        &height )
{
   printf( "getting dimensions... %u streams\n", ctx.nb_streams );
   width = height = 0 ;
   for( int i = 0 ; i < ctx.nb_streams ; i++ )
   {
      AVStream *const str = ctx.streams[i];
      if( CODEC_TYPE_VIDEO == str->codec.codec_type )
      {
         width = str->codec.width ;
         height = str->codec.height ;
         return ;
      }
      else
         fprintf( stderr, "stream %u is not video\n", i );
   }
}

void dumpStreams( AVFormatContext *ic_ptr )
{
   for( int i = 0 ; i < ic_ptr->nb_streams ; i++ )
   {
      AVStream *const str = ic_ptr->streams[i];
      printf( "stream %d -> codec %d (%s), type %s, w:%u, h:%u\n", 
              i, str->codec.codec_id,
              str->codec.codec_id < numCodecIds 
                  ? codecIds[str->codec.codec_id] 
                  : "unknown",
              codecTypes[str->codec.codec_type], 
              str->codec.width, str->codec.height );
   }
}

static void * table_rV[256];
static void * table_gU[256];
static int table_gV[256];
static void * table_bU[256];

static int div_round (int dividend, int divisor)
{
    if (dividend > 0)
	return (dividend + (divisor>>1)) / divisor;
    else
	return -((-dividend + (divisor>>1)) / divisor);
}

static uint32_t const matrix_coefficients = 6;

const int32_t Inverse_Table_6_9[8][4] = {
    {117504, 138453, 13954, 34903}, /* no sequence_display_extension */
    {117504, 138453, 13954, 34903}, /* ITU-R Rec. 709 (1990) */
    {104597, 132201, 25675, 53279}, /* unspecified */
    {104597, 132201, 25675, 53279}, /* reserved */
    {104448, 132798, 24759, 53109}, /* FCC */
    {104597, 132201, 25675, 53279}, /* ITU-R Rec. 624-4 System B, G */
    {104597, 132201, 25675, 53279}, /* SMPTE 170M */
    {117579, 136230, 16907, 35559}  /* SMPTE 240M (1987) */
};

static void yuv2rgb_c_init( void )
{
    int i;
    uint8_t table_Y[1024];
    uint32_t * table_32 = 0;
    uint16_t * table_16 = 0;
    uint8_t * table_8 = 0;
    int entry_size = 0;
    void * table_r = 0;
    void * table_g = 0;
    void * table_b = 0;

    int crv = Inverse_Table_6_9[matrix_coefficients][0];
    int cbu = Inverse_Table_6_9[matrix_coefficients][1];
    int cgu = -Inverse_Table_6_9[matrix_coefficients][2];
    int cgv = -Inverse_Table_6_9[matrix_coefficients][3];

    for (i = 0; i < 1024; i++) {
	int j;

	j = (76309 * (i - 384 - 16) + 32768) >> 16;
	j = (j < 0) ? 0 : ((j > 255) ? 255 : j);
	table_Y[i] = j;
    }

   table_16 = (uint16_t *) malloc ((197 + 2*682 + 256 + 132) *
                                  sizeof (uint16_t));
   
   entry_size = sizeof (uint16_t);
   table_r = table_16 + 197;
   table_b = table_16 + 197 + 685;
   table_g = table_16 + 197 + 2*682;
   
   for (i = -197; i < 256+197; i++) {
      int j = table_Y[i+384] >> 3;
   
      j <<= 11 ;
   
      ((uint16_t *)table_r)[i] = j;
   }
   for (i = -132; i < 256+132; i++) {
      int j = table_Y[i+384] >> 2 ;
   
      ((uint16_t *)table_g)[i] = j << 5;
   }
   for (i = -232; i < 256+232; i++) {
      int j = table_Y[i+384] >> 3;
   
      ((uint16_t *)table_b)[i] = j;
   }

    for (i = 0; i < 256; i++) {
	table_rV[i] = (((uint8_t *)table_r) +
		       entry_size * div_round (crv * (i-128), 76309));
	table_gU[i] = (((uint8_t *)table_g) +
		       entry_size * div_round (cgu * (i-128), 76309));
	table_gV[i] = entry_size * div_round (cgv * (i-128), 76309);
	table_bU[i] = (((uint8_t *)table_b) +
		       entry_size * div_round (cbu * (i-128), 76309));
    }
}


#define RGB(type,i) U = pu[i];   V = pv[i]; r = (type *) table_rV[V]; g = (type *) (((uint8_t *)table_gU[U]) + table_gV[V]); b = (type *) table_bU[U];
#define DST(py,dst,i) Y = py[2*i]; dst[2*i] = r[Y] + g[Y] + b[Y]; Y = py[2*i+1]; dst[2*i+1] = r[Y] + g[Y] + b[Y];
//
// output 2 lines of video to _dst1, _dst2
//
static void yuv2rgb_c_16 (uint8_t * py_1, uint8_t * py_2,
			  uint8_t * pu, uint8_t * pv,
			  void * _dst_1, void * _dst_2, int width)
{
    int U, V, Y;
    uint16_t * r, * g, * b;
    uint16_t * dst_1, * dst_2;

    width >>= 3;
    dst_1 = (uint16_t *) _dst_1;
    dst_2 = (uint16_t *) _dst_2;

    do {
	RGB (uint16_t, 0);
	DST (py_1, dst_1, 0);
	DST (py_2, dst_2, 0);

	RGB (uint16_t, 1);
	DST (py_2, dst_2, 1);
	DST (py_1, dst_1, 1);

	RGB (uint16_t, 2);
	DST (py_1, dst_1, 2);
	DST (py_2, dst_2, 2);

	RGB (uint16_t, 3);
	DST (py_2, dst_2, 3);
	DST (py_1, dst_1, 3);

	pu += 4;
	pv += 4;
	py_1 += 8;
	py_2 += 8;
	dst_1 += 8;
	dst_2 += 8;
    } while (--width);
}

typedef struct copyParams_t {
   unsigned char *dest_ ;
   unsigned w_ ;
   unsigned stride_ ;
   unsigned h_ ;
};

static void null_start (void * id, uint8_t * const * dest, int flags)
{
   copyParams_t *params = (copyParams_t *)id ;
   params->dest_ = dest[0];
}

static void null_copy( void * id, uint8_t * const * src,
		       unsigned int v_offset)
{
   copyParams_t const *params = (copyParams_t *)id ;

   uint8_t * dst;
   dst = params->dest_ + v_offset*(params->stride_);
   // copy luminosity (Y) into first half of buffer
   memcpy( dst, src[0], 8*params->stride_ );
   dst += 8*params->stride_ ;
   memcpy( dst, src[1], 2*params->stride_ );
   dst += 2*params->stride_ ;
   memcpy( dst, src[2], 2*params->stride_ );
}

static void nullslice_copy (void * id, uint8_t * const * src,
			    unsigned int v_offset)
{
   uint8_t * dst;
   uint8_t * py;
   uint8_t * pu;
   uint8_t * pv;
   int loop;

   copyParams_t const *params = (copyParams_t *)id ;

   dst = params->dest_ + v_offset*params->stride_ ;
   py = src[0]; pu = src[1]; pv = src[2];

//	id->uv_stride_frame = width >> 1;
//	id->rgb_stride_frame = ((bpp + 7) >> 3) * width;
   loop = 8;
   do {
      yuv2rgb_c_16( py, py + (params->stride_ >> 1), pu, pv,
                    dst, dst + params->stride_, params->w_ );
      py += params->stride_ ;
      pu += params->stride_ >> 2 ;
      pv += params->stride_ >> 2 ;
      dst += 2 * params->stride_ ;
   } while (--loop);
}

static void null_convert( int width, int height, uint32_t accel,
			  void * arg, convert_init_t * result)
{
   if( !result->id )
      result->id_size = sizeof(copyParams_t);
   else
   {
      copyParams_t *params = (copyParams_t *)result->id ;
      params->w_ = width ;
      params->stride_ = width*sizeof( unsigned short );
      params->h_ = height ;
      result->buf_size[0] = width*height*sizeof(unsigned short);
      result->buf_size[1] = result->buf_size[2] = 0;
      result->start = null_start;
      result->copy = null_copy;
//      result->copy = nullslice_copy;
   }
}

static INT64 bytesToUsec( int bytes,
                          int playbackHz,
                          int numChannels )
{
   INT64 result = (INT64)bytes * 1000000 ;
   result /= numChannels ;
   result /= playbackHz ;
   result /= sizeof( unsigned short );
   return result ;
}

static void *outThread( void *param )
{
   fbDevice_t &fbDev = getFB();

   mmQueue_t &queue = *( mmQueue_t * )param ;

   unsigned const fbStride = fbDev.getWidth() * sizeof( unsigned short );
   INT64 prevPTS  = 0 ; // PTS of previous audio clip
   INT64 prevTime = 0 ; // time of previous audio clip
   INT64 maxPTSDiff = 0 ;
   INT64 maxTimeDiff = 0 ;

   videoFrame_t *vFrame = 0 ;
   audioFrame_t *aFrame = 0 ;

   pollfd        filedes ;
   filedes.fd = queue.dspFd_ ;
   filedes.events = POLLOUT ;
   bool first = true ;

   while( !queue.shutdown_ )
   {
      if( !vFrame )
         queue.pullVideo( vFrame );
      if( !aFrame )
      {
         if( queue.pullAudio( aFrame ) )
         {
            INT64 when = pts_to_us( aFrame->when_, queue.ptsNumerator_, queue.ptsDenominator_ );
//            printf( "a pts %lld\n", aFrame->when_ );
         }
      }

      if( aFrame || vFrame )
      {
         while( aFrame )
         {
            audio_buf_info ai ;
            if( 0 == ioctl( queue.dspFd_, SNDCTL_DSP_GETOSPACE, &ai) ) 
            {
               if( 0 < ai.fragments )
               {
                  if( first || ( ai.fragments < ai.fragstotal ) )
                  {
                     first = false ;
                     write( queue.dspFd_, aFrame->data_, aFrame->length_ );

                     if( ( 0 != aFrame->when_ )
                         &&
                         ( prevPTS != aFrame->when_ ) )
                     {
                        if( aFrame->when_ < prevPTS )
                        {
                           printf( "back in time(PTS)\n" );
                        }
                        else
                        {
                           int qBytes ;
                           if( 0 == ioctl( queue.dspFd_, SNDCTL_DSP_GETODELAY, &qBytes ) ) 
                           {
                              prevPTS = aFrame->when_ ;
                              prevTime = now_us() + bytesToUsec( qBytes, queue.lastSampleRate_, queue.numAudioChannels_ );
                           }
                           else
                              fprintf( stderr, ":SNDCTL_DSP_GETODELAY:%m\n" );
                        }
                     }

                     queue.releaseAudioFrame();
                     if( queue.pullAudio( aFrame ) )
                     {
//                        INT64 when = pts_to_us( aFrame->when_, queue.ptsNumerator_, queue.ptsDenominator_ );
//                        printf( "a pts %lld\n", aFrame->when_ );
                     }
                     else 
                     {
                        if( ai.fragments > ai.fragstotal / 2 )
                           queue.displayFrames_ = PIC_MASK_CODING_TYPE_I ; // skip most
                        else
                           queue.displayFrames_ = PIC_MASK_CODING_TYPE_I | PIC_MASK_CODING_TYPE_P ; // skip B
                        break;
                     }
                  }
                  else
                  {
                     printf( "stall\n" );
                     queue.displayFrames_ = PIC_MASK_CODING_TYPE_I ; // skip most
                     if( queue.wait() )
                        first = true ;
                     break;
                  }
               } // space in output buffer
               else
               {
                  queue.displayFrames_ = PIC_MASK_ALLTYPES ; // display everything
//                  fprintf( stderr, "ws" );
                  break;
               }
            }
            else
            {
               fprintf( stderr, ":GETOSPACE:%m\n" );
               break;
            }
/*
            int const pollRes = poll( &filedes, 1, 0 );
            if( 0 < pollRes )
            {
            } // can write something
            else if( 0 > pollRes )
               fprintf( stderr, ":poll:%m\n" );
            else
               break;
*/               
         }

         while( vFrame )
         {
            if( ( 0 != prevPTS ) && ( 0 != vFrame->when_ ) )
            {
               if( vFrame->when_ >= prevPTS )
               {
                  INT64 ptsDiff = vFrame->when_ - prevPTS ;
                  INT64 timeDiff = pts_to_us( ptsDiff, queue.ptsNumerator_, queue.ptsDenominator_ );
                  if( ptsDiff > maxPTSDiff )
                  {
                     maxPTSDiff = ptsDiff ;
                     maxTimeDiff = timeDiff ;
                  }

                  INT64 when = prevTime + timeDiff ;
                  if( when > now_us() )
                  {
                     break;
                  }
               }
//               else
//                  printf( "backTimeVid\n" );
            }

            unsigned char *fbMem = (unsigned char *)fbDev.getMem();
            unsigned char const *src = vFrame->data_ ;
            for( unsigned i = 0 ; i < queue.height_ ; i++, fbMem += fbStride, src += queue.imgStride_ )
            {
               memcpy( fbMem, src, queue.imgStride_ );
            }

            queue.releaseVideoFrame();
            queue.pullVideo( vFrame );
         }
      }

      timespec const ts = { 0, 10000000 }; // 10 ms (1 tick)
      nanosleep( &ts, 0 );
   }
   
   printf( "max video diff %lld PTS, %lld uS\n", maxPTSDiff, maxTimeDiff );

   if( 0 != ioctl(queue.dspFd_, SNDCTL_DSP_SYNC, 0 ) ) 
      fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );

   return 0 ;
}

void main( int argc, char const * const argv[] )
{
   yuv2rgb_c_init();

   if( ( 2 == argc ) || ( 3 == argc ) )
   {
      int const flags = ( 3 == argc )
                        ? atoi( argv[2] )
                        : 3 ;

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
            dumpStreams( ic_ptr );
            
            mpeg2dec_t * mpeg2dec = mpeg2_init();
            if( mpeg2dec )
            {
               const mpeg2_info_t * const info = mpeg2_info( mpeg2dec );

               unsigned long numAudio = 0 ;
               unsigned long numVideo = 0 ;
               unsigned long numParse = 0 ;
               unsigned long numDraw  = 0 ;
               unsigned long numAudioOut = 0 ;
               unsigned long numSkipped = 0 ;

               AVPacket pkt;
               int mpState = mpeg2_parse( mpeg2dec );
               if( -1 == mpState )
               {
                  voQueue_t *voq = voQueueOpen();
                  if( voq )
                  {
                     vo_instance_t *output = &voq->vo ;

                     int const dspFd = open( "/dev/dsp", O_WRONLY );
                     if( 0 <= dspFd )
                     {
                        int const format = AFMT_S16_LE ;
                        if( 0 != ioctl( dspFd, SNDCTL_DSP_SETFMT, &format) ) 
                           fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );

/*
                        int const qBytes = 0 ;
                        if( 0 == ioctl( dspFd, SNDCTL_DSP_GETODELAY, &qBytes) ) 
                           printf( "odelay %d bytes\n", qBytes );
                        else
                           fprintf( stderr, ":ioctl(SNDCTL_DSP_GETODELAY):%m\n" );
*/

/*
                        int const vol = 0x5a5a ;
                        if( 0 > ioctl( dspFd, SOUND_MIXER_WRITE_VOLUME, &vol)) 
                           perror( "Error setting volume" );
*/
                        mmQueue_t outQueue( dspFd );
      
                        struct mad_stream stream;
                        mad_stream_init(&stream);
                        
                        struct mad_header header; 
                        bool haveMadHeader = false ;
                        struct mad_frame	frame;
                        struct mad_synth	synth;

                        time_t startSecs ; time( &startSecs );
                        unsigned long audioBytesOut = 0 ;
                        bool firstPicture = true ;
                        audioFrame_t *nextA = outQueue.nextAudioFrame();
                        nextA->when_   = 0 ;
                        nextA->length_ = 0 ;
                        unsigned short *nextOut = (unsigned short *)nextA->data_ ;

                        videoFrame_t *predFrames_[2];
                        bool skippedP = false ;
                        outQueue.ptsNumerator_   = ic_ptr->pts_num ;
                        outQueue.ptsDenominator_ = ic_ptr->pts_den ;

                        while( 1 )
                        {
                           int result = av_read_packet( ic_ptr, &pkt );
                           if( 0 <= result )
                           {
                              if( CODEC_TYPE_AUDIO == ic_ptr->streams[pkt.stream_index]->codec.codec_type )
                              {
                                 ++numAudio ;
                                 nextA->when_ = pkt.pts ;
                                 if( flags & ( 1<<CODEC_TYPE_AUDIO ) )
                                 {
                                    mad_stream_buffer( &stream, pkt.data, pkt.size );
                                    if( !haveMadHeader )
                                    {
                                       haveMadHeader = ( -1 != mad_header_decode( &header, &stream ) );
                                       if( haveMadHeader )
                                       {
                                          outQueue.numAudioChannels_ = MAD_NCHANNELS(&header);
                                          printf( "mad header: %u channels, %u Hz\n", outQueue.numAudioChannels_, header.samplerate );
                                          int const numChannels = 2 ;
                                          if( 0 != ioctl( dspFd, SNDCTL_DSP_CHANNELS, &numChannels ) )
                                             fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );
                                          mad_frame_init(&frame);
                                          mad_synth_init(&synth);
                                       }
                                    }
   
                                    if( haveMadHeader )
                                    {
                                       do {
                                          if( -1 != mad_frame_decode(&frame, &stream ) )
                                          {
                                             int sampleRate = frame.header.samplerate ;
                                             if( sampleRate != outQueue.lastSampleRate_ )
                                             {   
                                                outQueue.lastSampleRate_ = sampleRate ;
                                                if( 0 != ioctl( dspFd, SNDCTL_DSP_SPEED, &sampleRate ) )
                                                   fprintf( stderr, "Error setting sampling rate to %d:%m\n", sampleRate );
                                             }
                                             mad_synth_frame( &synth, &frame );
                                             numAudioOut++ ;
   
                                             if( 1 == outQueue.numAudioChannels_ )
                                             {
                                                mad_fixed_t const *left = synth.pcm.samples[0];
   
                                                for( unsigned i = 0 ; i < synth.pcm.length ; i++ )
                                                {
                                                   unsigned short const sample = scale( *left++ );
                                                   *nextOut++ = sample ;
                                                   *nextOut++ = sample ;
                                                   nextA->length_ += 2*sizeof(sample) ;
                                                   if( nextA->length_ >= sizeof( nextA->data_ ) )
                                                   {
                                                      audioBytesOut += nextA->length_ ;
                                                      outQueue.postAudioFrame();
                                                      nextA = outQueue.nextAudioFrame();
                                                      nextA->when_ = 0 ;
                                                      nextA->length_ = 0 ;
                                                      nextOut = (unsigned short *)nextA->data_ ;
                                                   }
                                                }
                                             } // mono
                                             else
                                             {
                                                mad_fixed_t const *left  = synth.pcm.samples[0];
                                                mad_fixed_t const *right = synth.pcm.samples[1];
   
                                                for( unsigned i = 0 ; i < synth.pcm.length ; i++ )
                                                {
                                                   *nextOut++ = scale( *left++ );
                                                   *nextOut++ = scale( *right++ );
                                                   nextA->length_ += 2*sizeof(*nextOut);
                                                   if( nextA->length_ >= sizeof( nextA->data_ ) )
                                                   {
                                                      audioBytesOut += nextA->length_ ;
                                                      outQueue.postAudioFrame();
                                                      nextA = outQueue.nextAudioFrame();
                                                      nextA->when_ = 0 ;
                                                      nextA->length_ = 0 ;
                                                      nextOut = (unsigned short *)nextA->data_ ;
                                                   }
                                                }
                                             } // stereo
                                             if( 0 != nextA->length_ )
                                             {
                                                audioBytesOut += nextA->length_ ;
                                                outQueue.postAudioFrame();
                                                nextA = outQueue.nextAudioFrame();
                                                nextA->when_ = 0 ;
                                                nextA->length_ = 0 ;
                                                nextOut = (unsigned short *)nextA->data_ ;
                                             }
                                          } // frame decoded
                                          else
                                          {
                                             if( MAD_RECOVERABLE( stream.error ) )
                                                ;
                                             else 
                                                break;
                                          }
                                       } while( 1 );
                                    }
                                 } // want audio?
                              }
                              else if( CODEC_TYPE_VIDEO == ic_ptr->streams[pkt.stream_index]->codec.codec_type )
                              {
/**/
                                 ++numVideo ;
                                 if( flags & ( 1<<CODEC_TYPE_VIDEO ) )
                                 {
                                    mpeg2_buffer( mpeg2dec, pkt.data, pkt.data + pkt.size );
         
                                    do {
                                       ++numParse ;
                                       mpState = mpeg2_parse( mpeg2dec );
                                       switch( mpState )
                                       {
                                          case STATE_SEQUENCE:
                                          {
                                             vo_setup_result_t setup_result;
                                             if( 0 == output->setup( output, info->sequence->width, info->sequence->height, &setup_result ) ) 
                                             {
                                                outQueue.initVideo( info->sequence->width, info->sequence->height );
                                                mpeg2_convert( mpeg2dec, convert_rgb16, NULL );
//                                                mpeg2_convert( mpeg2dec, null_convert, NULL );
                                 		mpeg2_custom_fbuf (mpeg2dec, 0);
                                             }
                                             else
                                             {
                                                fprintf (stderr, "display setup failed\n");
                                                exit (1);
                                             }

                                             break;
                                          }
                                          case STATE_GOP:
                                          {
                                             
                                             break;
                                          }
                                          case STATE_PICTURE:
                                          {
                                             int picType = ( info->current_picture->flags & PIC_MASK_CODING_TYPE ) - 1;
                                             static char const picTypes_[] = { 'I', 'P', 'B', 'D' };

                                             bool skip = ( 0 == ( outQueue.displayFrames_ & (1 << picType ) ) );

                                             if( PIC_FLAG_CODING_TYPE_P - 1 == picType ) 
                                             {
                                                if( skip )
                                                   skippedP = true ;
                                                else if( skippedP )
                                                   skip = true ;
                                             }
                                             else if( PIC_FLAG_CODING_TYPE_I - 1 == picType )
                                                skippedP = false ;

                                             mpeg2_skip( mpeg2dec, skip );
                                             break;
                                          }
                                          case STATE_SLICE:
                                          case STATE_END:
                                          {
                                             ++numDraw ;
                                             if (info->display_fbuf) 
                                             {
                                                if( ( 0 != info->current_picture )
                                                    && 
                                                    ( 0 == ( info->current_picture->flags & PIC_FLAG_SKIP ) ) )
                                                {
                                                   firstPicture = false ;
                                                   videoFrame_t *vFrame = outQueue.nextVideoFrame();
                                                   if( vFrame )
                                                   {
                                                      unsigned char const *mpgData = info->display_fbuf->buf[0];
                                                      unsigned char *frameData = vFrame->data_ ;
                                                      for( unsigned i = 0 ; i < outQueue.height_ ; i++ )
                                                      {
                                                         memcpy( frameData, mpgData, outQueue.imgStride_ );
                                                         mpgData   += outQueue.mpgStride_ ;
                                                         frameData += outQueue.imgStride_ ;
                                                      }
                                                      vFrame->when_ = pkt.pts ;
                                                      outQueue.postVideoFrame();
                                                   }
                                                }
                                                else
                                                {
                                                   ++numSkipped ;
                                                } // skipping frame
                                             }
                                             break;
                                          }
                                       }
                                    } while( -1 != mpState );
                                 }
                              }
                           }
                           else 
                           {
                              printf( "eof %d\n", result );
                              break;
                           }
                        }

                        printf( "shutting down\n" );
                        outQueue.shutdown();

                        time_t end ; time( &end );

                        mad_stream_finish(&stream);
   
                        close( dspFd );
                        unsigned elapsed = end-startSecs ;
                        printf( "%u seconds: %u pictures, %lu bytes of audio\n",
                                elapsed, numDraw, audioBytesOut );
                     }
                     else
                        perror( "/dev/dsp" );
                  }
                  else
                     fprintf( stderr, "Error opening frame buffer driver\n" );
               }
               else
                  fprintf( stderr, "mp2 initialization error\n" );

               printf( "%lu audio, %lu out\n", numAudio, numAudioOut );
               printf( "%lu video frames, %lu dropped\n", numVideo, numSkipped );
               printf( "%lu parse iterations, %lu draw cmds\n", numParse, numDraw );
               mpeg2_close( mpeg2dec );
            }
            else
               fprintf( stderr, "Error allocating mpeg2 decoder\n" );
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


static unsigned const VIDEOQUEUESIZE = 32 ;
static unsigned const VIDEOQUEUEMASK = (VIDEOQUEUESIZE-1);

static unsigned const AUDIOQUEUESIZE = 32 ;
static unsigned const AUDIOQUEUEMASK = (AUDIOQUEUESIZE-1);

mmQueue_t :: mmQueue_t( int fdDsp )
   : tHandle_( (pthread_t)-1 ),
     mpgWidth_( 0 ),
     mpgHeight_( 0 ),
     mpgStride_( 0 ),
     width_( 0 ),
     height_( 0 ),
     imgStride_( 0 ),
     fbStride_( 0 ),
     vFrameSize_( 0 ),
     frameMem_( 0 ),
     videoFrames_( 0 ),
     videoFrameAdd_( 0 ),
     videoFrameTake_( 0 ),
     audioMem_( new unsigned char [ AUDIOQUEUESIZE * sizeof( audioFrame_t ) ] ),
     audioFrames_( new audioFrame_t *[ AUDIOQUEUESIZE ] ),
     audioFrameAdd_( 0 ),
     audioFrameTake_( 0 ),
     dspFd_( fdDsp ),
     ptsNumerator_( 0 ),
     ptsDenominator_( 1 ),
     numAudioChannels_( 1 ),
     lastSampleRate_( -1 ),
     displayFrames_( PIC_MASK_CODING_TYPE ),
     shutdown_( false )
{
   pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER ; 
   mutex_ = m2 ; 
   pthread_cond_init( &feedVideoCond_, 0 );
   pthread_cond_init( &feedAudioCond_, 0 );
   pthread_cond_init( &pullCond_, 0 );
   audioFrame_t *nextFrame = (audioFrame_t *)audioMem_ ;
   for( unsigned i = 0 ; i < AUDIOQUEUESIZE ; i++, nextFrame++ )
   {
      audioFrames_[i] = nextFrame ;
   }
}

mmQueue_t :: ~mmQueue_t( void )
{
   if( frameMem_ )
      delete [] frameMem_ ;
   if( videoFrames_ )
      delete [] videoFrames_ ;
}

void mmQueue_t :: initVideo
   ( unsigned short width, 
     unsigned short height )
{
   if( frameMem_ )
      delete [] frameMem_ ;
   if( videoFrames_ )
      delete [] videoFrames_ ;

   mpgWidth_      = width ;
   mpgHeight_     = height ;
   mpgStride_     = width*sizeof( unsigned short );

   fbDevice_t &fb = getFB();

   if( width > fb.getWidth() )
      width = fb.getWidth();
   if( height > fb.getHeight() )
      height = fb.getHeight();

   width_         = width ;
   height_        = height ;
   imgStride_     = width*sizeof( unsigned short );
   vFrameSize_    = height*imgStride_+sizeof( INT64 );
   frameMem_      = new unsigned char [ vFrameSize_*VIDEOQUEUESIZE ];
   videoFrames_   = new videoFrame_t * [VIDEOQUEUESIZE];

   printf( "width %u, height %u\n", width, height );
   printf( "%u frames at %p (0x%x bytes each -> 0x%x bytes)\n", 
           VIDEOQUEUESIZE, frameMem_, vFrameSize_, vFrameSize_*VIDEOQUEUESIZE );
   unsigned char *nextVFrame = frameMem_ ;
   for( unsigned i = 0 ; i < VIDEOQUEUESIZE ; i++, nextVFrame += vFrameSize_ )
      videoFrames_[i] = (videoFrame_t *)nextVFrame ;
}

void mmQueue_t :: startThread( void )
{
   if( (pthread_t)-1 == tHandle_ )
   {
      int const create = pthread_create( &tHandle_, 0, outThread, this );
      if( 0 == create )
      {
         struct sched_param tsParam ;
         tsParam.__sched_priority = 90 ;
         pthread_setschedparam( tHandle_, SCHED_FIFO, &tsParam );
      }
      else
         fprintf( stderr, "Error %m creating output thread\n" );
   }
   else
   {
      pthread_cond_signal( &pullCond_ );
   }
}

videoFrame_t *mmQueue_t :: nextVideoFrame( void )
{
   videoFrame_t *rVal = 0 ;
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      if( ( ( videoFrameAdd_ + 1 ) & VIDEOQUEUEMASK ) == videoFrameTake_ )
      {
         startThread();
//         printf( "nV\n" );
      } // full
      else
      {
         unsigned short frame = videoFrameAdd_ ;

         rVal = videoFrames_[ frame ];
      }
      
      pthread_mutex_unlock( &mutex_ );
      
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
   return rVal ;
}

void mmQueue_t :: postVideoFrame( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      videoFrameAdd_ = ( videoFrameAdd_ + 1 ) & VIDEOQUEUEMASK ;
      pthread_mutex_unlock( &mutex_ );
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
}

audioFrame_t *mmQueue_t :: nextAudioFrame( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      while( ( ( audioFrameAdd_ + 1 ) & AUDIOQUEUEMASK ) == audioFrameTake_ )
      {
         startThread();
//         printf( "nA\n" );
         pthread_cond_wait( &feedAudioCond_, &mutex_ );
      }

      unsigned short frame = audioFrameAdd_ ;
      audioFrame_t *returnVal = audioFrames_[ frame ];

      pthread_mutex_unlock( &mutex_ );
      returnVal->length_ = 0 ;
      returnVal->when_   = 0 ;
      return returnVal ;
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
}

void mmQueue_t :: postAudioFrame( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      audioFrameAdd_ = ( audioFrameAdd_ + 1 ) & AUDIOQUEUEMASK ;
      pthread_mutex_unlock( &mutex_ );
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
}

bool mmQueue_t :: pullAudio( audioFrame_t *&aframe )
{
   aframe = 0 ;
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      if( audioFrameAdd_ != audioFrameTake_ )
         aframe = audioFrames_[audioFrameTake_];

      pthread_mutex_unlock( &mutex_ );
   
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }

   return 0 != aframe ;
}

bool mmQueue_t :: pullVideo( videoFrame_t *&vframe )
{
   vframe = 0 ;
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      if( videoFrameAdd_ != videoFrameTake_ )
         vframe = videoFrames_[videoFrameTake_];

      pthread_mutex_unlock( &mutex_ );
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }

   return 0 != vframe ;
}

bool mmQueue_t :: wait( void )
{
   bool haveOne = false ;
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      do {
         do
         {
         } while( 0 );
         haveOne = ( videoFrameAdd_ != videoFrameTake_ )
                   ||
                   ( audioFrameAdd_ != audioFrameTake_ );
         if( !( haveOne || shutdown_ ) )
            pthread_cond_wait( &pullCond_, &mutex_ );
      } while( !( haveOne || shutdown_ ) );

      pthread_mutex_unlock( &mutex_ );
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
   return haveOne ;
}

void mmQueue_t :: releaseVideoFrame( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      videoFrameTake_ = (videoFrameTake_+1) & VIDEOQUEUEMASK ;
      
      pthread_cond_signal( &feedVideoCond_ );
      pthread_mutex_unlock( &mutex_ );
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
}

void mmQueue_t :: releaseAudioFrame( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      audioFrameTake_ = (audioFrameTake_+1) & AUDIOQUEUEMASK ;
      
      pthread_cond_signal( &feedAudioCond_ );
      pthread_mutex_unlock( &mutex_ );
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
}

void mmQueue_t :: shutdown( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      shutdown_ = true ;
      pthread_cond_signal( &pullCond_ );
      pthread_mutex_unlock( &mutex_ );
   }
   if( (pthread_t)-1 != tHandle_ )
   {
      void *exitStat ;
      pthread_join( tHandle_, &exitStat );
   }
}

