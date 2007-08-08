/*
 * Module mpegQueue.cpp
 *
 * This module defines the methods of the mpegQueue_t
 * class as declared in mpegQueue.h
 *
 *
 * Change History : 
 *
 * $Log: mpegQueue.cpp,v $
 * Revision 1.23  2007-08-08 17:08:22  ericn
 * -[sm501] Use class names from sm501-int.h
 *
 * Revision 1.22  2007/07/07 19:25:09  ericn
 * -remove old trace references
 *
 * Revision 1.21  2007/01/03 22:07:22  ericn
 * -additional level of indirection (mediaQueue_t)
 *
 * Revision 1.20  2006/10/20 01:05:37  ericn
 * -don't TRACEMAD by default
 *
 * Revision 1.19  2006/10/16 22:33:30  ericn
 * -added aBytesQueued_ member, method, fixed header timestamps
 *
 * Revision 1.18  2006/09/17 15:54:48  ericn
 * -use unbuffered I/O for stream
 *
 * Revision 1.17  2006/09/12 13:40:37  ericn
 * -add resetAudio method
 *
 * Revision 1.16  2006/09/10 01:16:51  ericn
 * -trace events
 *
 * Revision 1.15  2006/09/06 15:09:58  ericn
 * -add utility routines for use in streaming
 *
 * Revision 1.14  2006/09/05 03:59:54  ericn
 * -fix time calculation in playAudio()
 *
 * Revision 1.13  2006/09/05 03:56:07  ericn
 * -allow B frames before starting, after GOP
 *
 * Revision 1.12  2006/09/05 03:53:37  ericn
 * -limit scope of queue locks during startup
 *
 * Revision 1.11  2006/09/05 02:29:14  ericn
 * -added queue dumping utilities for debug, lock identification
 *
 * Revision 1.10  2006/09/04 16:43:42  ericn
 * -use audio timing
 *
 * Revision 1.9  2006/09/04 15:16:06  ericn
 * -add audio support
 *
 * Revision 1.8  2006/09/01 22:52:19  ericn
 * -change to finer granularity for buffer sizes (er..time)
 *
 * Revision 1.7  2006/09/01 00:49:47  ericn
 * -change names to lowWater_ms() and highWater_ms()
 *
 * Revision 1.6  2006/08/30 02:10:53  ericn
 * -add statistics
 *
 * Revision 1.5  2006/08/27 19:14:44  ericn
 * -use mpegFileStream_t
 *
 * Revision 1.4  2006/08/26 17:12:51  ericn
 * -clean decoder on input discontinuity
 *
 * Revision 1.3  2006/08/26 16:11:32  ericn
 * Moved inlines into .cpp module
 * Added bookkeeping of allocations
 * Reworked MPEG decoder buffering to free allocations
 * Reworked test program to support multiple file names on the command line
 * Added output rectangle parameter
 *
 * Revision 1.2  2006/08/25 14:54:19  ericn
 * -add timing support
 *
 * Revision 1.1  2006/08/25 00:29:45  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

// #define DEBUGPRINT

#include "mpegQueue.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "yuyv.h"
#include "debugPrint.h"
#include "tickMs.h"
#include <string.h>
#include "fbDev.h"
#include <sys/soundcard.h>

// #define USE_MMAP

#ifdef MD5OUTPUT
#undef MD5OUTPUT
#endif

#ifdef MD5OUTPUT
#include <openssl/md5.h>
#endif

#define AUDIOSAMPLESPERFRAME 4096
unsigned int in_file_read = 0 ;

mpegQueue_t::mpegQueue_t
   ( int      dspFd,
     int      yuvFd,
     unsigned msToBuffer,
     rectangle_t const &outRect )
   : mediaQueue_t( dspFd, yuvFd, msToBuffer, outRect )
   , decoder_( mpeg2_init() )
{
}

mpegQueue_t::~mpegQueue_t( void )
{
   cleanup();

   if( decoder_ )
      mpeg2_close(decoder_);
}
   
void mpegQueue_t::feedAudio
   ( unsigned char const *data, 
     unsigned             length,
     bool                 discontinuity,
     long long            when_ms ) // transport PTS
{
   audioDecoder_.feed( data, length );

   while( audioDecoder_.getData() ){
debugPrint( "%s\n", audioDecoder_.timerString() );
      do {
         if( 0 == audioPartial_ ){
            audioPartial_ = getAudioBuf();
            assert( 0 != audioPartial_ );
            audioPartial_->header_.when_ms_ = when_ms ;
         }
         unsigned numFree = AUDIOSAMPLESPERFRAME-audioPartial_->length_ ;
         unsigned numRead ;
         unsigned short *next = audioPartial_->data_ + audioPartial_->length_ ;

         if( audioDecoder_.readSamples( next, numFree, numRead ) ){
            audioPartial_->length_ += numRead ;
            if( AUDIOSAMPLESPERFRAME == audioPartial_->length_ ){
               audioPartial_->sampleRate_ = audioDecoder_.sampleRate();
               audioPartial_->numChannels_ = audioDecoder_.numChannels();
               audioPartial_->offset_ = 0 ;

               if( 0LL == firstAudioMs_ ){
                  firstAudioMs_ = audioPartial_->header_.when_ms_ ;
debugPrint( "first audio frame: %lld\n", firstAudioMs_ );
               }
               lastAudioMs_ = audioPartial_->header_.when_ms_ ;
               audioPartial_->header_.when_ms_ += audioOffs_ ;
debugPrint( "audio %lld\n", audioPartial_->header_.when_ms_ );

               queueLock_t lockAudioQ( &audioFull_.locked_, 1 );
               assert( lockAudioQ.weLocked() ); // only reader side should fail

               pushTail( audioFull_.header_, &audioPartial_->header_ );
               audioPartial_ = 0 ;
               ++aFramesQueued_ ;
               aBytesQueued_ += 2*AUDIOSAMPLESPERFRAME ;

               if( highWater_ms()*4 <= audioTailFromNow() ){
                  printf( "------> too much audio queued: %lu/%lu\n", highWater_ms(), audioTailFromNow() );
                  printf( "flags = 0x%lx\n", flags_ );
                  dumpStats();
                  dumpAudioQueue();
                  exit(1);
               }
            }
         }
         else {
            break ;
         }
      } while( 1 );
   } // while more output synthesized
if( flags_ & AUDIOIDLE )
   playAudio(tickMs());
}

void mpegQueue_t::addDecoderBuf()
{
   videoEntry_t *const ve = getPictureBuf();
   unsigned char *buf = ve->data_ ;
   unsigned ySize = inBufferLength_/2 ; 
   unsigned uvSize = ySize / 2 ;
   unsigned char *planes[3] = { 
      buf,
      buf+ySize,
      buf+ySize+uvSize
   };
   mpeg2_set_buf(decoder_, planes, ve);
   pushTail( decoderBufs_.header_, &ve->header_ );
}

static char const cPicTypes[] = {
   "xIPBD"
};

void mpegQueue_t::feedVideo
   ( unsigned char const *data, 
     unsigned             length,
     bool                 discontinuity,
     long long            offset_ms ) // 
{
   if( discontinuity ){
      if( 0 == (flags_ & NEEDIFRAME)){
         flags_ |= NEEDIFRAME ;
      }
      mpeg2_reset( decoder_, 1 );
      cleanDecoderBufs();
   }

   mpeg2_buffer( decoder_, (uint8_t *)data, (uint8_t *)data + length );

   mpeg2_info_t const *const infoptr = mpeg2_info( decoder_ );
   int mpState = -1 ;
   do {
      mpState = mpeg2_parse( decoder_ );

      switch( mpState )
      {
         case STATE_SEQUENCE:
         {
            mpeg2_sequence_t const &seq = *infoptr->sequence ;
            if( ( seq.width != inWidth_ )
                ||
                ( seq.height != inHeight_ ) ){
               inWidth_ = seq.width ;
               inHeight_ = seq.height ;
               inStride_ = ((seq.width+15)/16)*16 ;
               inBufferLength_ = inHeight_*inStride_*2;
            }

            unsigned frameRate ;
            switch( seq.frame_period )
            {
               case 1126125 :	// 1 (23.976) - NTSC FILM interlaced
               case 1125000 :	// 2 (24.000) - FILM
                  frameRate = 24 ;
                  break;
               case 1080000 :	// 3 (25.000) - PAL interlaced
                  frameRate = 25 ;
                  break;
               case  900900 : // 4 (29.970) - NTSC color interlaced
               case  900000 : // 5 (30.000) - NTSC b&w progressive
                  frameRate = 30 ;
                  break;
               case  540000 : // 6 (50.000) - PAL progressive
                  frameRate = 50 ;
                  break;
               case  450450 : // 7 (59.940) - NTSC color progressive
               case  450000 : // 8 (60.000) - NTSC b&w progressive
                  frameRate = 60 ;
                  break;
               default:
                  frameRate = 0 ;
            }

            if( ( 0 != frameRate ) && ( inRate_ != frameRate ) )
               msPerPic_ = 1000/frameRate ;

            mpeg2_convert( decoder_, mpeg2convert_yuyv, NULL );
            mpeg2_custom_fbuf(decoder_, 1);
            for( unsigned i = 0 ; i < 2 ; i++ ){
               addDecoderBuf();
            }

            break;

         } // SEQUENCE

         case STATE_PICTURE:
         {
            int picType = ( infoptr->current_picture->flags & PIC_MASK_CODING_TYPE );
            if( flags_ & STARTED ){
               if( msVideoQueued() <= lowWater_ms() ){
                  debugPrint( "skipping b-frames w/%lu ms queued\n", msVideoQueued() );
                  flags_ |= NEEDIFRAME ;
               }
            }

            if( ( 0 == ( flags_ & NEEDIFRAME ) )
                ||
                ( PIC_FLAG_CODING_TYPE_I == picType ) 
                ||
                ( PIC_FLAG_CODING_TYPE_P == picType ) )
            {
debugPrint( "%c", cPicTypes[picType] );
               mpeg2_skip( decoder_, 0 );
               addDecoderBuf();

               if( 0 != (flags_ & NEEDIFRAME) ){
                  if( ( 0 == (flags_ & STARTED) ) 
                      ||
                      ( msVideoQueued() >= lowWater_ms() ) ){
                     flags_ &= ~NEEDIFRAME ;
                     debugPrint( "play b-frames...\n" );
                  }
               }
            }
            else {
               debugPrint( "skip picture type %c\n", cPicTypes[picType] );
               mpeg2_skip( decoder_, 1 );
               msOut_ += msPerPic_ ;
               ++vFramesDropped_ ;
            }
            
            break;
         } // PICTURE
         case STATE_END:
         case STATE_INVALID_END:
         case STATE_SLICE:
         {
            if( (0 != infoptr->display_fbuf)
                &&
                (0 != infoptr->current_picture)
                && 
                ( 0 == ( infoptr->current_picture->flags & PIC_FLAG_SKIP ) ) )
            {
               videoEntry_t *const ve = (videoEntry_t *)infoptr->display_fbuf->id ;
               if( ve )
               {
assert( ve->width_ == inWidth_ );
assert( ve->height_ == inHeight_ );
assert( ve->length_ == inBufferLength_ );
                  ve->header_.when_ms_ = msOut_ ;
                  msOut_ += msPerPic_ ;
                  unlink(ve->header_);
                  queuePicture( ve );
               }
               else
                  fprintf( stderr, "Invalid fbuf id!\n" );
            }

            if( mpState != STATE_SLICE ){
               cleanDecoderBufs();
            }
            break;
         } // SLICE
         case STATE_GOP:
debugPrint( "\n%8lld - G", offset_ms );
            if( 0 != offset_ms )
               msOut_ = offset_ms ;
            if( 0 != (flags_ & NEEDIFRAME)){
               flags_ &= ~NEEDIFRAME ;
            }

            break ;
         case STATE_BUFFER :
         case STATE_SEQUENCE_REPEATED:
            break ;
         case STATE_INVALID:
            mpeg2_reset(decoder_,1);
            cleanDecoderBufs();
            break ;
         default: 
            debugPrint( "unknown state: %d\n", mpState );
      } // switch
      debugPrint( "out_mpeg2_pars: state == %d\n", mpState );

      if( 0 == (flags_ & STARTED) )
      {
          if( ( msVideoQueued() >= bufferMs_ )
              ||
              ( msAudioQueued() >= bufferMs_ ) ){
             startPlayback();
          }
      }
   } while( ( -1 != mpState ) && ( STATE_BUFFER != mpState ) );
}

#ifdef MODULETEST

#include "mpDemux.h"
#include "memFile.h"

static char const * const frameTypes_[] = {
   "video",
   "audio",
   "endOfFile"
};

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <zlib.h>
#include "tickMs.h"
#include <termios.h>
#include <linux/sm501yuv.h>
#include <mpeg2dec/mpeg2.h>
#include "mpegDecode.h"
#include <stdio.h>
#include <linux/sm501yuv.h>
#include <sys/ioctl.h>
#include <linux/sm501-int.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "trace.h"
#include "rtSignal.h"
#include <signal.h>
#include "mpegStream.h"
#include <zlib.h>

typedef struct stats_t {
   unsigned ticks_ ;
   unsigned idle_ ;
   unsigned inFileRead_ ;
   unsigned inParse_ ;
#ifdef TRACEMPEG2
   unsigned inSlice_ ;
   unsigned inConvert_ ; 
   unsigned idctRow_ ;
   unsigned idctCol_ ;
   unsigned idctCopy_ ;
   unsigned idctAdd_ ;
   unsigned idctInit_ ;
   unsigned inMe_ ; // motion estimation
#endif
   unsigned inWrite_ ;
   unsigned inAudioFeed_ ;
   unsigned inAudioSynth_ ;
   unsigned inAudioSynthFrame_ ;
   unsigned inAudioPull_ ;
#ifdef TRACEMAD
   unsigned inSynthFull_ ;
   unsigned inSynthHalf_ ;
   unsigned inSynthDCT_ ;
#endif
};



static void traceCallback( void *param )
{
   stats_t *stats = (stats_t *)param ;
   stats->ticks_++ ;
   bool isIdle = true ;
   if( in_file_read ){
      stats->inFileRead_++ ;
      isIdle = false ;
   }
#if 0   
   if( in_mpeg2_parse ){
      stats->inParse_++ ;
      isIdle = false ;
   }
   if( in_write ){
      stats->inWrite_++ ;
      isIdle = false ;
   }
   if( in_mp3_feed ){
      stats->inAudioFeed_++ ;
      isIdle = false ;
   }
   if( in_mp3_synth ){
      stats->inAudioSynth_++ ;
      isIdle = false ;
   }
#endif
   if( in_synth_frame ){
      stats->inAudioSynthFrame_++ ;
      isIdle = false ;
   }
#if 0   
   if( in_mp3_pull ){
      stats->inAudioPull_++ ;
      isIdle = false ;
   }
#endif
#ifdef TRACEMPEG2
   if( in_mpeg2_slice ){
      stats->inSlice_++ ;
      isIdle = false ;
   }
   if( in_mpeg2_convert ){
      stats->inConvert_++ ;
      isIdle = false ;
   }
   if( in_mpeg2_dct_row ){
      stats->idctRow_++ ;
      isIdle = false ;
   }
   if( in_mpeg2_dct_col ){
      stats->idctCol_++ ;
      isIdle = false ;
   }
   if( in_mpeg2_dct_copy ){
      stats->idctCopy_++ ;
      isIdle = false ;
   }
   if( in_mpeg2_dct_add ){
      stats->idctAdd_++ ;
      isIdle = false ;
   }
   if( in_mpeg2_dct_init ){
      stats->idctInit_++ ;
      isIdle = false ;
   }
   if( in_mpeg2_me ){
      stats->inMe_++ ;
      isIdle = false ;
   }
#endif
#ifdef TRACEMAD
   if( in_mad_synth_full ){
      stats->inSynthFull_++ ;
      isIdle = false ;
   }
   if( in_mad_synth_half ){
      stats->inSynthHalf_++ ;
      isIdle = false ;
   }
   if( in_mad_synth_dct ){
      stats->inSynthDCT_++ ;
      isIdle = false ;
   }
#endif
   if( isIdle )
      stats->idle_++ ;
}

static mpegQueue_t *inst_ = 0 ;

static void vsyncHandler( int signo, siginfo_t *info, void *context )
{
   if( inst_ ){
      long long now = tickMs();
      inst_->playVideo(now);
   }
}

static void audioHandler( int signo, siginfo_t *info, void *context )
{
   if( inst_ ){
      long long now = tickMs();
      inst_->playAudio(now);
   }
}

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      int dspFd = open( "/dev/dsp", O_WRONLY );
      if( 0 > dspFd ){
         fprintf( stderr, "/dev/dsp:%m\n" );
         exit(1);
      }
      
      if( 0 != ioctl(dspFd, SNDCTL_DSP_SYNC, 0 ) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );

      int const format = AFMT_S16_LE ;
      if( 0 != ioctl( dspFd, SNDCTL_DSP_SETFMT, &format) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );

      int const channels = 2 ;
      if( 0 != ioctl( dspFd, SNDCTL_DSP_CHANNELS, &channels ) )
         fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );

      int speed = 44100 ;
      if( 0 != ioctl( dspFd, SNDCTL_DSP_SPEED, &speed ) )
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", speed );

      int flags = fcntl(dspFd, F_GETFL);
      fcntl(dspFd, F_SETFL, flags | O_NONBLOCK | FASYNC );

      int vol = (75<<8)|75 ;
      if( 0 > ioctl( dspFd, SOUND_MIXER_WRITE_VOLUME, &vol)) 
         perror( "Error setting volume" );

      unsigned xPos = 0 ;
      unsigned yPos = 0 ;
      unsigned outWidth  = 0 ;
      unsigned outHeight = 0 ;
      unsigned picTypeMask = (unsigned)-1 ;
      if( 2 < argc )
      {
         xPos = (unsigned)strtoul( argv[2], 0, 0 );
         if( 3 < argc )
         {
            yPos = (unsigned)strtoul( argv[3], 0, 0 );
            if( 4 < argc )
            {
               outWidth = (unsigned)strtoul( argv[4], 0, 0 );
               if( 5 < argc )
               {
                  outHeight = (unsigned)strtoul( argv[5], 0, 0 );
                  if( 6 < argc ){
                     picTypeMask = (unsigned)strtoul( argv[6], 0, 0 );
                     printf( "picTypeMask == 0x%08X\n", picTypeMask );
                  }
               }
            }
         }
      }

      int const fdYUV = open( "/dev/" SM501YUV_CLASS, O_WRONLY );
      if( 0 > fdYUV ){
         perror( "/dev/" SM501YUV_CLASS );
         return -1 ;
      }
      fcntl(fdYUV, F_SETFL, fcntl(fdYUV, F_GETFL) | O_NONBLOCK | FASYNC );

      stats_t stats ;
      memset( &stats, 0, sizeof(stats) );
      startTrace( traceCallback, &stats );

      int const pid_ = getpid();
      int const vsyncSignal = nextRtSignal();
      int const fdSync = open( "/dev/" SM501INT_CLASS, O_RDONLY );
      if( 0 > fdSync ){
         perror( "/dev/" SM501INT_CLASS );
         exit(-1);
      }
      fcntl(fdSync, F_SETOWN, pid_);
      fcntl(fdSync, F_SETSIG, vsyncSignal );

      struct sigaction sa ;

      sa.sa_flags = SA_SIGINFO|SA_RESTART ;
      sa.sa_restorer = 0 ;
      sigemptyset( &sa.sa_mask );
      sa.sa_sigaction = vsyncHandler ;
      sigaddset( &sa.sa_mask, vsyncSignal );
      sigaction(vsyncSignal, &sa, 0 );
      
      int const audioSignal = nextRtSignal();
      fcntl(dspFd, F_SETOWN, pid_ );
      fcntl(dspFd, F_SETSIG, audioSignal );

      sigemptyset( &sa.sa_mask );
      sa.sa_sigaction = audioHandler ;
      sigaddset( &sa.sa_mask, audioSignal );
      sigaction(audioSignal, &sa, 0 );

      fbDevice_t &fb = getFB();
      rectangle_t outRect ;
      outRect.xLeft_ = outRect.yTop_ = 0 ;
      outRect.width_ = fb.getWidth();
      outRect.height_ = fb.getHeight();

      mpegQueue_t q( dspFd, fdYUV, 2000, outRect );
      inst_ = &q ;
      
      flags = fcntl( fdSync, F_GETFL, 0 );
      fcntl( fdSync, F_SETFL, flags | O_NONBLOCK | FASYNC );

      for( int arg = 1 ; arg < argc ; arg++ ){
         char const *const fileName = argv[arg];
         FILE *fIn = fopen( fileName, "rb" );
         if( fIn )
         {
printf( "----> playing file %s\n", fileName );
            long long startMs = tickMs();

            // only ready 1/2 in normal case, to allow read of tail-end
            // if necessary
            unsigned long adler = 0 ;
            bool firstVideo = true ;
   
            mpegStreamFile_t stream( fileno(fIn) );
            unsigned char const *frame ;
            unsigned             frameLen ;
            CodecType type ;
            CodecID codecId ;
            long long pts ;
            unsigned char streamId ;

in_file_read = 1 ;
            while( stream.getFrame( frame, frameLen, pts, streamId, type, codecId ) )
            {
in_file_read = 0 ;
               adler = adler32( adler, frame, frameLen );
//               printf( "%d/%u/%08lX/%08lx\n", type, frameLen, adler32( 0, frame, frameLen ), adler );
               bool isVideo = ( CODEC_TYPE_VIDEO == type );
               
               adler = adler32( adler, frame, frameLen );

               if( isVideo ){
                  while( q.highWater_ms() <= q.msVideoQueued() ){
                     pause();
                  }
                  if( 1 != arg ){
                     if( firstVideo && ( 0LL != pts ) )
                        q.adjustPTS( q.ptsToMs(pts) );
                  }
                  q.feedVideo( frame, frameLen, false, q.ptsToMs(pts) );
                  if( firstVideo && (0LL != pts) )
                     firstVideo = false ;
               } else {
                  q.feedAudio( frame, frameLen, false, q.ptsToMs(pts) );
               }
            }
in_file_read = 0 ;

            if( ( q.msVideoQueued() > 2*q.highWater_ms() )
                &&
                ( q.msAudioQueued() > 2*q.highWater_ms() ) ){
               printf( "invalid queue lengths:\n"
                       "%lu ms video, %lu ms audio\n",
                       q.msVideoQueued(), q.msAudioQueued() );
            }

            if( !q.started() )
               q.startPlayback();

            while( ( 0 < q.msVideoQueued() ) || ( 0 < q.msAudioQueued() ) ){
               pause();
            }

            // wait for audio empty
            do {
               audio_buf_info ai ;
               if( 0 == ioctl( dspFd, SNDCTL_DSP_GETOSPACE, &ai) ){
                  if( ai.fragments == ai.fragstotal ){
                     long long emptyTime = tickMs();
                     printf( "empty at %lld (%lu ms after first write)\n", 
                             emptyTime, (unsigned long)( emptyTime-q.firstAudioWrite() ) );
                     break ;
                  }
               }
               else {
                  perror( "GETOSPACE" );
                  break ;
               }
            } while( 1 );

            long long endMs = tickMs();
            printf( "%lu ms elapsed\n"
                    "%u allocations\n"
                    "%u frees\n", 
                    (unsigned long)(endMs-startMs),
                    q.numAllocated(),
                    q.numFreed() );

            fclose( fIn );
         }
         else
            perror( argv[arg] );
      }
      q.cleanup();

      printf( "after all files:\n"
              "%u allocations\n"
              "%u frees\n", 
              q.numAllocated(),
              q.numFreed() );

      unsigned traceCount ;
      traceEntry_t *traces = endTrace( traceCount );
      dumpTraces( traces, traceCount );
      delete [] traces ;

      q.dumpStats();
      printf( "%4u ticks\n"
              "%4u idle (%u.%02u%%)\n"
              "%4u reading file\n"
              "%4u parsing\n"
              "%4u feeding audio\n"
              "%4u audio synth\n"
              "%4u synth frame\n"
              "%4u pulling audio\n"
#ifdef TRACEMAD
              "%4u mad synth full\n"
              "%4u mad synth half\n"
              "%4u mad synth DCT\n"
#endif
#ifdef TRACEMPEG2                    
              "%4u slicing\n"
              "%4u converting\n"
              "      %4u idct row\n"
              "      %4u idct col\n"
              "      %4u idct copy\n"
              "      %4u idct add\n"
              "      %4u idct init\n"
              "%4u motion estimation\n"
#endif
              "%4u writing\n",
              stats.ticks_,
              stats.idle_,
              ((stats.idle_*100)/stats.ticks_),
              ((stats.idle_*10000)/stats.ticks_)%100,
              stats.inFileRead_,
              stats.inParse_,
              stats.inAudioFeed_,
              stats.inAudioSynth_,
              stats.inAudioSynthFrame_,
              stats.inAudioPull_,
#ifdef TRACEMAD
              stats.inSynthFull_,
              stats.inSynthHalf_,
              stats.inSynthDCT_,
#endif
#ifdef TRACEMPEG2                    
              stats.inSlice_,
              stats.inConvert_,
              stats.idctRow_,
              stats.idctCol_,
              stats.idctCopy_,
              stats.idctAdd_,
              stats.idctInit_,
              stats.inMe_,
#endif
              stats.inWrite_ );
      if( 0 <= fdYUV )
         close( fdYUV );
   }
   else
      fprintf( stderr, "Usage: mpegDecode fileName [x [,y [,width [,height]]]]\n" );

   return 0 ;
}


#endif
