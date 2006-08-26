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
 * Revision 1.4  2006-08-26 17:12:51  ericn
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
#include <sys/ioctl.h>
#include <linux/sm501yuv.h>
#include <linux/sm501-int.h>
#include <string.h>
#include "fbDev.h"

#ifdef MD5OUTPUT
#undef MD5OUTPUT
#endif

#ifdef MD5OUTPUT
#include <openssl/md5.h>
#endif

extern "C" {
#ifdef TRACEMPEG2
extern int volatile in_mpeg2_slice ;
extern int volatile in_mpeg2_convert ;
extern int volatile in_mpeg2_dct_row ;
extern int volatile in_mpeg2_dct_col ;
extern int volatile in_mpeg2_dct_copy ;
extern int volatile in_mpeg2_dct_add ;
extern int volatile in_mpeg2_dct_init ;
extern int volatile in_mpeg2_me ;
#endif
int volatile in_mpeg2_parse = 0 ;
int volatile in_write = 0 ;
};


#define fieldsize( __st, __f ) sizeof( ((__st const *)0)->__f )

#define AUDIOFRAMESIZE 4096

class queueLock_t {
public:
   queueLock_t( unsigned volatile *lock );
   ~queueLock_t( void );

   bool weLocked( void ) const { return 0 == prevVal_ ; }
   bool wasLocked( void ) const { return 0 != prevVal_ ; }

private:
   queueLock_t( queueLock_t const & );
   unsigned volatile *lock_ ;
   unsigned           prevVal_ ;
};

queueLock_t::queueLock_t( unsigned volatile *lock )
   : lock_( lock )
{
   debugPrint( "locking: %p/%d...", this, *lock );
   unsigned old ;
   unsigned newval = 1 ;
   __asm__ __volatile__ (
      "swp %0, %1, [%2]"
      : "=&r" (old)
      : "r" (newval), "r" (lock)
      : "memory", "cc");
   prevVal_ = old ;
   debugPrint( "%d\n", *lock );
}

queueLock_t::~queueLock_t( void )
{
   if( weLocked() ){
      debugPrint( "unlocking: %p/%d...", this, *lock_ );
      unsigned ret ;
      unsigned newval = 0 ;
      unsigned volatile *lock = lock_ ; // need it in a register

      __asm__ __volatile__ (
         "swp %0, %1, [%2]"
   	 : "=&r" (ret)
   	 : "r" (newval), "r" (lock)
   	 : "memory", "cc");
      assert( 1 == ret );
      debugPrint( "%d\n", *lock );
   }
   else
      debugPrint( "don't unlock\n" );
}

bool mpegQueue_t::isEmpty( entryHeader_t const &eh )
{
   return ( eh.prev_ == eh.next_ )
          &&
          ( eh.prev_ == &eh );
}
   
mpegQueue_t::entryHeader_t *mpegQueue_t::pullHead( entryHeader_t &eh )
{
   entryHeader_t *e = eh.next_ ;
   if( e != &eh ){
      e->next_->prev_ = &eh ;
      eh.next_ = e->next_ ;
      
      e->next_ = e->prev_ = e ;     // decouple
      return e ;
   }

   return 0 ;
}

void mpegQueue_t::pushTail( entryHeader_t &head, 
                            entryHeader_t *entry )
{
   assert( entry );
   assert( entry->next_ == entry );
   assert( entry->prev_ == entry );

   entry->prev_ = head.prev_;
   entry->next_ = &head ;
   head.prev_->next_ = entry ;
   head.prev_ = entry ;
}

void mpegQueue_t::freeEntry( entryHeader_t *e )
{
   assert( e && ( e->next_ == e ) && ( e->prev_ == e ) );
   delete [] (char *)e ;
}
   
void mpegQueue_t::unlink( entryHeader_t &entry )
{
   assert( entry.next_ && ( entry.next_ != &entry ) );
   assert( entry.prev_ && ( entry.prev_ != &entry ) );
   entry.next_->prev_ = entry.prev_ ;
   entry.prev_->next_ = entry.next_ ;

   entry.next_ = entry.prev_ = &entry ;
}

mpegQueue_t::mpegQueue_t
   ( int      dspFd,
     int      yuvFd,
     unsigned bufferSeconds,
     rectangle_t const &outRect )
   : dspFd_( dspFd )
   , yuvFd_( yuvFd )
   , outRect_( outRect )
   , bufferMs_( bufferSeconds*1000 )
   , halfBuffer_(bufferMs_/2)
   , doubleBuffer_(bufferMs_*2)
   , flags_( 0 )
   , prevOutWidth_( 0 )
   , prevOutHeight_( 0 )
   , prevOutBufferLength_( 0 )
   , yuvOut_( 0 )
   , inWidth_( 0 )
   , inHeight_( 0 )
   , inStride_( 0 )
   , inBufferLength_( 0 )
   , inRate_( 0 )
   , msPerPic_( 0 )
   , msOut_( 0LL )
   , startMs_( 0LL )
   , allocCount_( 0 )
   , freeCount_( 0 )
   , decoder_( mpeg2_init() )
{
printf( "fds: dsp(%d), yuv(%d)\n", dspFd_, yuvFd_ );   
   videoFull_.locked_ = 0 ;
   videoFull_.header_.prev_ 
      = videoFull_.header_.next_ 
      = &videoFull_.header_ ;
   videoEmpty_.locked_ = 0 ;
   videoEmpty_.header_.prev_ 
      = videoEmpty_.header_.next_ 
      = &videoEmpty_.header_ ;
   decoderBufs_.locked_ = 0 ;
   decoderBufs_.header_.prev_ 
      = decoderBufs_.header_.next_ 
      = &decoderBufs_.header_ ;
   audioFull_.locked_ = 0 ;
   audioFull_.header_.prev_ 
      = audioFull_.header_.next_ 
      = &audioFull_.header_ ;
   audioEmpty_.locked_ = 0 ;
   audioEmpty_.header_.prev_ 
      = audioEmpty_.header_.next_ 
      = &audioEmpty_.header_ ;

#ifdef MD5OUTPUT
   MD5_Init( &videoMD5_ );
#endif
}

void mpegQueue_t::cleanup( void )
{
   cleanDecoderBufs();
   lockAndClean( decoderBufs_ );
   lockAndClean( videoFull_ );
   lockAndClean( videoEmpty_ );
   lockAndClean( audioFull_ );
   lockAndClean( audioEmpty_ );
}

mpegQueue_t::~mpegQueue_t( void )
{
   cleanup();

   if( decoder_ )
      mpeg2_close(decoder_);
#ifdef MD5OUTPUT
   unsigned char md5[MD5_DIGEST_LENGTH];
   MD5_Final( md5, &videoMD5_ );

   printf( "   md5: " );
   for( unsigned i = 0 ; i < MD5_DIGEST_LENGTH ; i++ ){
      printf( "%02x ", md5[i] );
   }
   printf( "\n" );
#endif
}
   
void mpegQueue_t::lockAndClean( queueHeader_t &qh )
{
   queueLock_t lockVideoQ( &qh.locked_ );
   assert( lockVideoQ.weLocked() );
   cleanQueue(qh);
}

void mpegQueue_t::cleanQueue( queueHeader_t &qh )
{
   assert( qh.locked_ );
   entryHeader_t *next ;
   while( 0 != ( next = pullHead( qh.header_ ) ) ){
      freeEntry( next );
   }
}

void mpegQueue_t::feedAudio
   ( unsigned char const *data, 
     unsigned             length,
     bool                 discontinuity,
     long long            pts ) // transport PTS
{
}


mpegQueue_t::videoEntry_t *mpegQueue_t::getPictureBuf(void)
{
   queueLock_t lockVideoQ( &videoEmpty_.locked_ );
   assert( lockVideoQ.weLocked() ); // only reader side should fail

   entryHeader_t *next ;
   while( 0 != ( next = pullHead( videoEmpty_.header_ ) ) ){
      videoEntry_t *const ve = (videoEntry_t *)next ;
      if( ve->length_ == inBufferLength_ ){
debugPrint( "recycle buffer: %p\n", ve );
         ve->width_ = inWidth_ ;
         ve->height_ = inHeight_ ;
         ++allocCount_ ;
         return ve ;
      }
      else {
         freeEntry( next );
      }
   }

   unsigned size = sizeof(videoEntry_t)
                  -fieldsize(videoEntry_t,data_)
                  +inBufferLength_ ;
debugPrint( "allocate buffer of size : %u\n", size );
   ++allocCount_ ;

   unsigned char * const data = new unsigned char [ size ];
   videoEntry_t *const ve = (videoEntry_t *)data ;
   ve->length_ = inBufferLength_ ;
   ve->width_ = inWidth_ ;
   ve->height_ = inHeight_ ;
   ve->header_.next_ = ve->header_.prev_ = &ve->header_ ;

   return ve ;
}

void mpegQueue_t::startPlayback( void )
{
   if( 0 == ( flags_ & STARTED ) ){
      flags_ |= STARTED ;
   
      long long const now = tickMs();

      queueLock_t lockVideoQ( &videoFull_.locked_ );
      assert( lockVideoQ.weLocked() );

      entryHeader_t *entry = videoFull_.header_.next_ ;
      startMs_ = now - entry->when_ms_ ;

      while( entry != &videoFull_.header_ ){
         entry->when_ms_ += startMs_ ;
         entry = entry->next_ ;
      }

      queueLock_t lockAudioQ( &audioFull_.locked_ );
      assert( lockAudioQ.weLocked() );

      entry = audioFull_.header_.next_ ;
      while( entry != &audioFull_.header_ ){
         entry->when_ms_ += startMs_ ;
         entry = entry->next_ ;
      }
   }
}

void mpegQueue_t::adjustPTS( long long startPts )
{
   if( flags_ & STARTED ){
      long long nowPlus = tickMs() + bufferMs_ - startPts ;

      startMs_ = nowPlus ;
      msOut_ = startPts ;
   }
}

void mpegQueue_t::queuePicture(videoEntry_t *ve)
{
   queueLock_t lockVideoQ( &videoFull_.locked_ );
   assert( lockVideoQ.weLocked() ); // only reader side should fail

   ve->header_.when_ms_ += startMs_ ;

   pushTail( videoFull_.header_, &ve->header_ );
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

void mpegQueue_t::cleanDecoderBufs()
{
   queueLock_t lockEmptyQ( &videoEmpty_.locked_ );
   assert( lockEmptyQ.weLocked() );

   entryHeader_t *e ;
   while( 0 != ( e = pullHead( decoderBufs_.header_ ) ) ){
      ++freeCount_ ;
      pushTail( videoEmpty_.header_, e );
   }
}

void mpegQueue_t::feedVideo
   ( unsigned char const *data, 
     unsigned             length,
     bool                 discontinuity,
     long long            offset_ms ) // 
{
   if( ( 0LL != offset_ms )
       &&
       ( 0LL == msOut_ ) )
      msOut_ = offset_ms ;

   if( discontinuity ){
      flags_ |= NEEDIFRAME ;
      mpeg2_reset( decoder_, 1 );
      cleanDecoderBufs();
   }

   mpeg2_buffer( decoder_, (uint8_t *)data, (uint8_t *)data + length );

   mpeg2_info_t const *const infoptr = mpeg2_info( decoder_ );
   int mpState = -1 ;
   do {
      in_mpeg2_parse = 1 ;
      mpState = mpeg2_parse( decoder_ );
      in_mpeg2_parse = 0 ;

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
                if( msVideoQueued() <= msHalfBuffer() ){
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
               debugPrint( "picture type %d\n", picType );
               mpeg2_skip( decoder_, 0 );
               debugPrint( "add decoder buf\n" );
               addDecoderBuf();

               debugPrint( "flags %lx\n", flags_ );

               if( ( PIC_FLAG_CODING_TYPE_I == picType )
                   &&
                   ( 0 != (flags_ & NEEDIFRAME) ) ){

                  if( msVideoQueued() >= msToBuffer() ){
                     flags_ &= ~NEEDIFRAME ;
                     debugPrint( "play b-frames...\n" );
                  }
               }
            }
            else {
               debugPrint( "skip picture type %d\n", picType );
               mpeg2_skip( decoder_, 1 );
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
         case STATE_BUFFER :
         case STATE_SEQUENCE_REPEATED:
         case STATE_GOP:
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
          if( msVideoQueued() >= bufferMs_ ){
             startPlayback();
          }
      }
   } while( ( -1 != mpState ) && ( STATE_BUFFER != mpState ) );

}

void mpegQueue_t::playAudio
   ( long long when ) // in ms a.la. tickMs()
{
   if( flags_ & STARTED ){
      queueLock_t lockAudioQ( &audioFull_.locked_ );
      if( lockAudioQ.weLocked() ){
         while( !isEmpty( audioFull_.header_ ) ){
            audioEntry_t *const ae = (audioEntry_t *)audioFull_.header_.next_ ;
            if( 0 <= ( when-ae->header_.when_ms_ ) ){
               int numWritten = write( dspFd_, ae->data_+ae->offset_, ae->length_ );
               if( 0 < numWritten ){
                  ae->length_ -= numWritten ;
                  if( 0 == ae->length_ ){
                     entryHeader_t *const e = pullHead( audioFull_.header_ );
                     assert( e );
                     assert( e == (entryHeader_t *)ae );
                     pushTail( audioEmpty_.header_, e );
                     continue ;
                  } // finished this packet of audio
                  else
                     ae->offset_ += numWritten ;
               }
               else
                  fprintf( stderr, "Short audio write: %d/%d/%u: %m\n", numWritten, errno, ae->length_ );
            } // time to play

            // continue from the middle
            break ;
         }
      }
      else
         flags_ |= AUDIOIDLE ;
   } // if we've started playback
}

unsigned long mpegQueue_t::msVideoQueued( void ) const 
{
   entryHeader_t const * const head = videoFull_.header_.next_ ;
   entryHeader_t const * const tail = videoFull_.header_.prev_ ;
   unsigned long queueTime = (tail->when_ms_ - head->when_ms_);

   debugPrint( "%lu: %p/%p: %lld..%lld\n", queueTime, head, tail, head->when_ms_, tail->when_ms_ );
   return (unsigned long)( queueTime );
}

extern "C" {
   void memcpyAlign4( void *dst, void const *src, unsigned len );
};

void mpegQueue_t::playVideo( long long when )
{
   if( flags_ & STARTED ){
      queueLock_t lockVideoQ( &videoFull_.locked_ );
      queueLock_t lockEmptyQ( &videoEmpty_.locked_ );
      if( lockVideoQ.weLocked() && lockEmptyQ.weLocked() ){
         while( !isEmpty( videoFull_.header_ ) ){
            videoEntry_t *const ve = (videoEntry_t *)videoFull_.header_.next_ ;
            if( 0 <= ( when-ve->header_.when_ms_ ) ){
               // peek ahead (maybe we can skip a write
               if( ( ve->header_.next_ != &videoFull_.header_ )
                   &&
                   ( 0 <= ( when-ve->header_.next_->when_ms_ ) ) ){
                  debugPrint( "peek ahead: %llu/%llu/%llu (%lu ms queued)\n", 
                              when, 
                              ve->header_.when_ms_, 
                              ve->header_.next_->when_ms_, 
                              msVideoQueued() );
                  entryHeader_t *const e = pullHead( videoFull_.header_ );
                  assert( e );
                  assert( e == (entryHeader_t *)ve );

#ifdef MD5OUTPUT
   MD5_Update(&videoMD5_, ve->data_, ve->length_ );
#endif

                  ++freeCount_ ;
                  pushTail( videoEmpty_.header_, e );
                  continue ;
               } // skip: more than one frame is ready
               else {
                  if( ( ve->width_ != prevOutWidth_)
                      ||
                      ( ve->height_ != prevOutHeight_ ) ){
                     prevOutWidth_ = ve->width_ ;
                     prevOutHeight_ = ve->height_ ;
                     prevOutBufferLength_ = ve->length_ ;

                     struct sm501yuvPlane_t plane ;
                     plane.xLeft_     = outRect_.xLeft_ ;
                     plane.yTop_      = outRect_.yTop_ ;
                     plane.inWidth_   = ve->width_ ;
                     plane.inHeight_  = ve->height_ ;
                     plane.outWidth_  = outRect_.width_ ;
                     plane.outHeight_ = outRect_.height_ ;

                     if( 0 != ioctl( yuvFd_, SM501YUV_SETPLANE, &plane ) )
                     {
                        perror( "setPlane" );
                        exit(-1);
                     }
                     else {
                        debugPrint( "setPlane success\n"
                                    "%u:%u %ux%u -> %ux%u\n"
                                    "offset == 0x%x\n", 
                                    plane.xLeft_, plane.yTop_,
                                    plane.inWidth_, plane.inHeight_,
                                    plane.outWidth_, plane.outHeight_,
                                    plane.planeOffset_ );
                        fbDevice_t &fb = getFB();
                        yuvOut_ = (unsigned *)( (char *)fb.getMem() + plane.planeOffset_ );
                        reg_and_value rv ;
                        rv.reg_ = SMIVIDEO_CTRL ;
                        rv.value_ = SMIVIDEO_CTRL_ENABLE_YUV ;
                        int res = ioctl( fb.getFd(), SM501_WRITEREG, &rv );
			               if( 0 == res )
                           debugPrint( "enabled YUV\n" );
                        else
                           debugPrint( "Error enabling YUV\n" );
                     }
                  }

#if 1
                  assert( 0 != yuvOut_ );
                  in_write = 1 ;
                  
                  unsigned numCacheLines = ve->length_ / 32 ;
                  unsigned long const *nextIn = (unsigned long const *)ve->data_ ;
                  unsigned long *nextOut = (unsigned long *)yuvOut_ ;

                  __asm__ volatile (
                  "  pld   [%0, #32]\n"
                  "  pld   [%0, #64]\n"
                  "  pld   [%0, #96]\n"
                  :
                  : "r" (nextIn)
                  );

                  while( 0 < numCacheLines-- ){
                    __asm__ volatile (
                     "  pld   [%0, #32]\n"
                     "  pld   [%0, #64]\n"
                     "  pld   [%0, #96]\n"
                     "  pld   [%0, #128]\n"
                     :
                     : "r" (nextIn)
                    );
                    nextOut[0] = nextIn[0];
                    nextOut[1] = nextIn[1];
                    nextOut[2] = nextIn[2];
                    nextOut[3] = nextIn[3];
                    nextOut[4] = nextIn[4];
                    nextOut[5] = nextIn[5];
                    nextOut[6] = nextIn[6];
                    nextOut[7] = nextIn[7];

                    nextOut += 8 ;
                    nextIn += 8 ;
                  }

                  in_write = 0 ;

                  entryHeader_t *const e = pullHead( videoFull_.header_ );
                  assert( e );
                  assert( e == (entryHeader_t *)ve );
                  ++freeCount_ ;
                  pushTail( videoEmpty_.header_, e );
                  continue ;
#else

                  in_write = 1 ;
                  int numWritten = write( yuvFd_, ve->data_, ve->length_ );
                  in_write = 0 ;
                  if( ve->length_ == (unsigned)numWritten ){
                     entryHeader_t *const e = pullHead( videoFull_.header_ );
                     assert( e );
                     assert( e == (entryHeader_t *)ve );
                     ++freeCount_ ;
                     pushTail( videoEmpty_.header_, e );
                     continue ;
                  }
                  else
                     fprintf( stderr, "Short video write: %d/%d/%u: %m\n", numWritten, errno, ve->length_ );
#endif
               } // no peek ahead
            } // time to play
            else
               debugPrint( "idle: %llu/%llu\n", when, ve->header_.when_ms_ );

            // continue from the middle
            break ;
         }
      }
      else {
         debugPrint( "not locked\n" );
      }
   } // if we've started playback
   else
      debugPrint( "idle\n" );
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
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include "trace.h"
#include "rtSignal.h"
#include <signal.h>
#include "mpegStream.h"
#include <zlib.h>

typedef struct stats_t {
   unsigned ticks_ ;
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
};



static void traceCallback( void *param )
{
   stats_t *stats = (stats_t *)param ;
   stats->ticks_++ ;
   if( in_mpeg2_parse )
      stats->inParse_++ ;
   if( in_write )
      stats->inWrite_++ ;
#ifdef TRACEMPEG2
   if( in_mpeg2_slice )
      stats->inSlice_++ ;
   if( in_mpeg2_convert )
      stats->inConvert_++ ;
   if( in_mpeg2_dct_row )
      stats->idctRow_++ ;
   if( in_mpeg2_dct_col )
      stats->idctCol_++ ;
   if( in_mpeg2_dct_copy )
      stats->idctCopy_++ ;
   if( in_mpeg2_dct_add )
      stats->idctAdd_++ ;
   if( in_mpeg2_dct_init )
      stats->idctInit_++ ;
   if( in_mpeg2_me )
      stats->inMe_++ ;
#endif
}

static mpegQueue_t *inst_ = 0 ;

static void vsyncHandler( int signo, siginfo_t *info, void *context )
{
   if( inst_ ){
      long long now = tickMs();
      inst_->playVideo(now);
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

      int const fdYUV = open( "/dev/yuv", O_WRONLY );
      if( 0 > fdYUV ){
         perror( "/dev/yuv" );
         return -1 ;
      }

      stats_t stats ;
      memset( &stats, 0, sizeof(stats) );
      startTrace( traceCallback, &stats );

      int const pid_ = getpid();
      int const vsyncSignal = nextRtSignal();
      int const fdSync = open( "/dev/sm501vsync", O_RDONLY );
      if( 0 > fdSync ){
         perror( "/dev/sm501sync" );
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
      
      fbDevice_t &fb = getFB();
      rectangle_t outRect ;
      outRect.xLeft_ = outRect.yTop_ = 0 ;
      outRect.width_ = fb.getWidth();
      outRect.height_ = fb.getHeight();

      mpegQueue_t q( dspFd, fdYUV, 1, outRect );
      inst_ = &q ;
      
      int flags = fcntl( fdSync, F_GETFL, 0 );
      fcntl( fdSync, F_SETFL, flags | O_NONBLOCK | FASYNC );

      for( int arg = 1 ; arg < argc ; arg++ ){
         char const *const fileName = argv[arg];
         FILE *fIn = fopen( fileName, "rb" );
         if( fIn )
         {
printf( "----> playing file %s\n", fileName );
            long long startMs = tickMs();
   
            mpegStream_t mpStream ;
   
            // only ready 1/2 in normal case, to allow read of tail-end
            // if necessary
            unsigned char inBuf[8192];
            unsigned const inSize = sizeof(inBuf)/2 ;
   
            int globalOffs = 0 ;
            int numRead ;
            unsigned long adler = 0 ;
            bool firstVideo = true ;
   
            while( 0 < ( numRead = fread( inBuf+globalOffs, 1, inSize-globalOffs, fIn ) ) ){
               unsigned char const *nextIn = inBuf ;
               unsigned numLeft = numRead ;
   
               mpegStream_t::frameType_e frameType ;
               unsigned offset ;
               unsigned frameLen ;
               long long pts, dts ;
               unsigned char streamId ;
   
               while( // printf( "feed:%p/%u\n", nextIn, numLeft ) 
                      // &&
                      mpStream.getFrame( nextIn, numLeft,
                                         frameType,
                                         offset, frameLen, 
                                         pts, dts, streamId ) )
               {
                  assert( offset <= numLeft );
                  unsigned const end = offset+frameLen ;
                  if( end > (unsigned)numLeft ){
                     unsigned const needed = end-numLeft ;
                     assert( needed < inSize );
                     numRead = fread( (unsigned char *)nextIn+numLeft, 1, needed, fIn );
                     assert( numRead == (int)needed );
                     numLeft = end ;
                  } // read the rest of this packet
   
                  unsigned char const *frameStart = nextIn+offset ;
                  adler = adler32( adler, frameStart, frameLen );
   //printf( "%d/%u/%08lX/%08lx\n", frameType, frameLen, adler32( 0, frameStart, frameLen ), adler );
   
                  if( mpegStream_t::videoFrame_e == frameType ){
                     while( q.msDoubleBuffer() <= q.msVideoQueued() )
                        pause();
                     if( 1 != arg ){
                        if( firstVideo && ( 0LL != pts ) )
                           q.adjustPTS( q.ptsToMs(pts) );
                     }
                     q.feedVideo( frameStart, frameLen, firstVideo, q.ptsToMs(pts) );
                     if( firstVideo && (0LL != pts) )
                        firstVideo = false ;
                  } else if( mpegStream_t::audioFrame_e == frameType ){
                     q.feedAudio( frameStart, frameLen, false, q.ptsToMs(pts) );
                  }
                  else
                     printf( "unknown frame type: %d\n", frameType );
   
                  nextIn += end ;
                  numLeft -= end ;
               }
   
               unsigned const used = nextIn - inBuf ;
               if( used < (unsigned)numRead ){
                  globalOffs = numRead - (nextIn-inBuf);
               }
               else
                  globalOffs = 0 ;
   
               if( globalOffs )
                  memcpy( inBuf, nextIn, globalOffs );
            }

            while( 0 < q.msVideoQueued() )
               pause();

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

      printf( "%u ticks\n"
              "%u parsing\n"
#ifdef TRACEMPEG2                    
              "%u slicing\n"
              "%u converting\n"
              "      %u idct row\n"
              "      %u idct col\n"
              "      %u idct copy\n"
              "      %u idct add\n"
              "      %u idct init\n"
              "%u motion estimation\n"
#endif
              "%u writing\n",
              stats.ticks_,
              stats.inParse_,
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
