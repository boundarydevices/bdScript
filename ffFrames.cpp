#define HAVE_LRINTF
extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/dsputil.h"
#include "mpeg2dec/mpeg2.h"
#include "mpeg2dec/video_out.h"
#include "../mpeg2dec-0.3.1/libmpeg2/mpeg2_internal.h"
extern vo_open_t vo_fb_open ;
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
   return ((INT64)now.tv_sec)|(INT64)now.tv_usec ;
}

static void us_sleep( INT64 delta_us )
{
   timespec ts ;
   ts.tv_sec = 0 ;
   ts.tv_nsec = delta_us*1000 ;
//   nanosleep( &ts, 0 );
}

struct videoFrame_t {
   INT64         when_ ;
   unsigned char data_[1];
};

struct audioFrame_t {
   INT64         when_ ;
   unsigned      length_ ;
   unsigned char data_[8192];
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
   videoFrame_t &nextVideoFrame( void );
   void          postVideoFrame( void );
   
   audioFrame_t *nextAudioFrame( void );
   void          postAudioFrame( void );
   
   void          shutdown( void );


   //
   // read side
   //
   bool pull( bool  &isVideo, void *&frame );
   void releaseVideoFrame();
   void releaseAudioFrame();

// private:
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

static void *outThread( void *param )
{
   fbDevice_t &fbDev = getFB();

   mmQueue_t &queue = *( mmQueue_t * )param ;
   bool  isVideo ;
   void *frame ;
   unsigned const fbStride = fbDev.getWidth() * sizeof( unsigned short );
   INT64 startTime = (INT64)0 ;
   INT64 lastTime  = (INT64)0 ;
   while( queue.pull( isVideo, frame ) )
   {
//      printf( "pulled frame %p\n", frame ); fflush( stdout );
      if( isVideo )
      {
         videoFrame_t const &vf = *( videoFrame_t * )frame ;
//         printf( "video at %llu\n", vf.when_ );
         unsigned char *fbMem = (unsigned char *)fbDev.getMem();
         unsigned char const *src = vf.data_ ;
         for( unsigned i = 0 ; i < queue.height_ ; i++, fbMem += fbStride, src += queue.imgStride_ )
         {
            memcpy( fbMem, src, queue.imgStride_ );
         }

         lastTime = vf.when_ ;
         queue.releaseVideoFrame();
         
      }
      else
      {
         audioFrame_t const &aFrame = *( audioFrame_t const * )frame ;
//         printf( "audio at %llu, length %lu\n", aFrame.when_, aFrame.length_ );
         lastTime = aFrame.when_ ;
         write( queue.dspFd_, aFrame.data_, aFrame.length_ );
         queue.releaseAudioFrame();
      }
      
      if( 0 == startTime )
         startTime = lastTime ;
   }

   printf( "start %lld, end %lld\n", startTime, lastTime );

   return 0 ;
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

void main( int argc, char const * const argv[] )
{
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
                        int const vol = 0x5a5a ;
                        if( 0 > ioctl( dspFd, SOUND_MIXER_WRITE_VOLUME, &vol)) 
                           perror( "Error setting volume" );
*/
                        mmQueue_t outQueue( dspFd );
      
                        pthread_t tHandle = (pthread_t) -1 ;
                        int const create = pthread_create( &tHandle, 0, outThread, &outQueue );
                        if( 0 == create )
                        {
                           struct sched_param tsParam ;
                           tsParam.__sched_priority = 90 ;
                           pthread_setschedparam( tHandle, SCHED_FIFO, &tsParam );
                        }
                        else
                           fprintf( stderr, "Error %m creating output thread\n" );

                        int lastSampleRate = -1 ;
                        int nChannels ;
                        struct mad_stream stream;
                        mad_stream_init(&stream);
                        
                        struct mad_header header; 
                        bool haveMadHeader = false ;
                        struct mad_frame	frame;
                        struct mad_synth	synth;

                        INT64 startPTS = 0 ;
                        INT64 playAt = 0 ;

                        time_t startSecs ; time( &startSecs );
                        unsigned long audioBytesOut = 0 ;

                        audioFrame_t *nextA = outQueue.nextAudioFrame();
                        nextA->when_   = playAt ;
                        nextA->length_ = 0 ;
                        unsigned short *nextOut = (unsigned short *)nextA->data_ ;

                        videoFrame_t *predFrames_[2];

                        while( 1 )
                        {
                           int result = av_read_packet( ic_ptr, &pkt );
                           if( 0 <= result )
                           {
                              if( (INT64)0 != pkt.pts )
                              {
                                 if( 0 == startPTS )
                                    startPTS = pkt.pts ;
                                 INT64 diffPts = pkt.pts - startPTS ;
//                                 INT64 diffus = ( ( diffPts*ic_ptr->pts_num )*1000000 ) / ic_ptr->pts_den ;
//                                 playAt = diffus ;
                                 playAt = diffPts ;
                              } // otherwise use previous value

                              if( CODEC_TYPE_AUDIO == ic_ptr->streams[pkt.stream_index]->codec.codec_type )
                              {
                                 ++numAudio ;
                                 if( flags & ( 1<<CODEC_TYPE_AUDIO ) )
                                 {
                                    mad_stream_buffer( &stream, pkt.data, pkt.size );
                                    if( !haveMadHeader )
                                    {
                                       haveMadHeader = ( -1 != mad_header_decode( &header, &stream ) );
                                       if( haveMadHeader )
                                       {
                                          nChannels = MAD_NCHANNELS(&header);
                                          printf( "mad header: %u channels, %u Hz\n", nChannels, header.samplerate );
                                          if( 0 != ioctl( dspFd, SNDCTL_DSP_CHANNELS, &nChannels ) )
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
                                             if( sampleRate != lastSampleRate )
                                             {   
                                                lastSampleRate = sampleRate ;
                                                if( 0 != ioctl( dspFd, SNDCTL_DSP_SPEED, &sampleRate ) )
                                                   fprintf( stderr, "Error setting sampling rate to %d:%m\n", sampleRate );
                                             }
                                             mad_synth_frame( &synth, &frame );
                                             numAudioOut++ ;
   
                                             if( 1 == nChannels )
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
                                                      nextA->when_ = playAt ;
                                                      nextA->length_ = 0 ;
                                                      nextOut = (unsigned short *)nextA->data_ ;
                                                   }
                                                }
                                             } // mono
                                             else
                                             {
                                                mad_fixed_t const *left  = synth.pcm.samples[0];
                                                mad_fixed_t const *right = synth.pcm.samples[1];
   // nextA->length_ = sizeof( nextA->data_ );
   
                                                for( unsigned i = 0 ; i < synth.pcm.length ; i++ )
                                                {
                                                   *nextOut++ = scale( *left++ );
                                                   *nextOut++ = scale( *right++ );
                                                   nextA->length_ += 4 ;
                                                   if( nextA->length_ >= sizeof( nextA->data_ ) )
                                                   {
                                                      audioBytesOut += nextA->length_ ;
                                                      outQueue.postAudioFrame();
                                                      nextA = outQueue.nextAudioFrame();
                                                      nextA->when_ = playAt ;
                                                      nextA->length_ = 0 ;
                                                      nextOut = (unsigned short *)nextA->data_ ;
                                                   }
                                                }
                                                
                                             } // stereo
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
/*
                                       switch( mpState )
                                       {
                                          case STATE_SEQUENCE:
                                             printf( "q" ); break ;
                                          case STATE_PICTURE:
                                             printf( "p" ); break ;
                                          case STATE_PICTURE_2ND:
                                             printf( "2" ); break ;
                                          case STATE_SLICE:
                                             printf( "l" ); break ;
                                          case STATE_END:
                                             printf( "e" ); break ;
                                          default:
                                             printf( "?" ); break ;
                                       }
*/
                                       switch( mpState )
                                       {
                                          case STATE_SEQUENCE:
                                          {
                                             vo_setup_result_t setup_result;
                                             if( 0 == output->setup( output, info->sequence->width, info->sequence->height, &setup_result ) ) 
                                             {
                                                outQueue.initVideo( info->sequence->width, info->sequence->height );
                                                if (setup_result.convert)
                                                   mpeg2_convert( mpeg2dec, setup_result.convert, NULL );
/*
                                                videoFrame_t &vFrame = outQueue.nextVideoFrame();
                                                unsigned char *bufs[3] = {
                                                   vFrame.data_, 0, 0
                                                };
                                 		mpeg2_custom_fbuf (mpeg2dec, 1);
                                 		mpeg2_set_buf (mpeg2dec, bufs, &vFrame);
                                 		mpeg2_set_buf (mpeg2dec, bufs, &vFrame);
*/                                                
                                             }
                                             else
                                             {
                                                fprintf (stderr, "display setup failed\n");
                                                exit (1);
                                             }
                                             
                                             break;
                                          }
                                          case STATE_PICTURE:
                                          {
/*
                                             videoFrame_t &vFrame = outQueue.nextVideoFrame();
                                             unsigned char *bufs[3] = {
                                                vFrame.data_, 0, 0
                                             };
                                             mpeg2_set_buf (mpeg2dec, bufs, &vFrame);
*/
                                             break;
                                          }
                                          case STATE_SLICE:
                                          case STATE_END:
                                             ++numDraw ;
                                             if (info->display_fbuf) {
/**/
                                                videoFrame_t &vFrame = outQueue.nextVideoFrame();
                                                vFrame.when_ = playAt ;
                                                voq->pixMem_ = vFrame.data_ ;
   // printf( "draw to %p\n", vFrame.data_ ); fflush( stdout );
                                                output->draw( output, info->display_fbuf->buf, info->display_fbuf->id );
   // printf( "done drawing\n" );fflush( stdout );
/**/   
                                                outQueue.postVideoFrame();
                                             }
                                       }
                                    } while( -1 != mpState );
   /**/
// printf( "\n" );
                                 }
                              }
                           }
                           else 
                           {
                              printf( "eof %d\n", result );
                              break;
                           }
                        }
                        
                        if( 0 != nextA->length_ )
                        {
                           audioBytesOut += nextA->length_ ;
                           outQueue.postAudioFrame();
                        }

                        outQueue.shutdown();
                        if( (pthread_t)-1 != tHandle )
                        {
                           void *exitStat ;
                           pthread_join( tHandle, &exitStat );
                        }

                        time_t end ; time( &end );

                        mad_stream_finish(&stream);
   
                        close( dspFd );
                        unsigned elapsed = end-startSecs ;
                        printf( "%u seconds: %u pictures, %lu bytes of audio\n",
                                elapsed, numDraw, audioBytesOut );
                     }
                     else
                        perror( "/dev/dsp" );
                     
                     if( output->close )
                        output->close( output );
                  }
                  else
                     fprintf( stderr, "Error opening frame buffer driver\n" );
               }
               else
                  fprintf( stderr, "mp2 initialization error\n" );

               printf( "%lu audio, %lu out\n", numAudio, numAudioOut );
               printf( "%lu video frames\n", numVideo );
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
   : width_( 0 ),
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

videoFrame_t &mmQueue_t :: nextVideoFrame( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      while( ( ( videoFrameAdd_ + 1 ) & VIDEOQUEUEMASK ) == videoFrameTake_ )
      {
         pthread_cond_wait( &feedVideoCond_, &mutex_ );
      }
      
      unsigned short frame = videoFrameAdd_ ;
      
      pthread_mutex_unlock( &mutex_ );
      
      return *videoFrames_[ frame ];
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
}

void mmQueue_t :: postVideoFrame( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      videoFrameAdd_ = ( videoFrameAdd_ + 1 ) & VIDEOQUEUEMASK ;
      pthread_cond_signal( &pullCond_ );
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
      pthread_cond_signal( &pullCond_ );
      pthread_mutex_unlock( &mutex_ );
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
}

bool mmQueue_t :: pull
   ( bool  &isVideo,
     void *&frame )
{
   bool haveOne = false ;
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      do {
         videoFrame_t *vFrame = 0 ;
         if( videoFrameAdd_ != videoFrameTake_ )
         {
            vFrame = videoFrames_[videoFrameTake_];
         }
         
         audioFrame_t *aFrame = 0 ;
         if( audioFrameAdd_ != audioFrameTake_ )
         {
            aFrame = audioFrames_[audioFrameTake_];
         }

         if( ( 0 == vFrame ) && ( 0 == aFrame ) )
         {
            if( !shutdown_ )
               pthread_cond_wait( &pullCond_, &mutex_ );
            else
               break;
         }
         else 
         {
            haveOne = true ;
            if( ( 0 != vFrame ) && ( 0 != aFrame ) )
            {
               if( aFrame->when_ < vFrame->when_ )
                  frame = aFrame ;
               else
                  frame = vFrame ;
            } // have both
            else if( 0 != vFrame )
               frame = vFrame ;
            else
               frame = aFrame ;
            isVideo = ( (void *)vFrame == frame );
         }
      } while( !haveOne );
      
      pthread_mutex_unlock( &mutex_ );
   
      return haveOne ;
   }
   else
   {
      fprintf( stderr, "Error locking mmQueue\n" );
      exit( 1 );
   }
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
      audioFrameTake_ = (audioFrameTake_+1) & VIDEOQUEUEMASK ;
      
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
}

