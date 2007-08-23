/*
 * Module mediaQueue.cpp
 *
 * This module defines the methods of the mediaQueue_t
 * class as declared in mediaQueue.h
 *
 * Change History : 
 *
 * $Log: mediaQueue.cpp,v $
 * Revision 1.4  2007-08-23 00:25:48  ericn
 * -add CLOEXEC for file handles
 *
 * Revision 1.3  2007/08/08 17:08:20  ericn
 * -[sm501] Use class names from sm501-int.h
 *
 * Revision 1.2  2007/05/03 20:50:37  ericn
 * -fix leak of partial audio queue
 *
 * Revision 1.1  2007/01/03 22:12:10  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "mediaQueue.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "debugPrint.h"
#include "tickMs.h"
#include <sys/ioctl.h>
#include <linux/sm501yuv.h>
#include <linux/sm501-int.h>
#include <string.h>
#include "fbDev.h"
#include <sys/soundcard.h>
// #define LOGTRACES
#include "logTraces.h"

// #define USE_MMAP

#ifdef MD5OUTPUT
#undef MD5OUTPUT
#endif

#ifdef MD5OUTPUT
#include <openssl/md5.h>
#endif

#define fieldsize( __st, __f ) sizeof( ((__st const *)0)->__f )

#define AUDIOSAMPLESPERFRAME 4096

queueLock_t::queueLock_t( 
   unsigned volatile *lock,
   unsigned id )
   : lock_( lock )
   , id_( id )
{
   debugPrint( "locking: %p/%d...", this, *lock );
   unsigned old ;
   unsigned newval = id_ ;
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
      if( 0 == ret ){
         printf( "failed unlock attempt for queue lock %p\n"
                 "id %u\n"
                 "prevVal %u\n"
                 "locked by %u\n", lock_, id_, prevVal_, ret );
         exit(1);
      }
      debugPrint( "%d\n", *lock );
   }
   else
      debugPrint( "don't unlock\n" );
}

mediaQueue_t::mediaQueue_t
   ( int      dspFd,
     int      yuvFd,
     unsigned msToBuffer,
     rectangle_t const &outRect )
   : dspFd_( dspFd )
   , yuvFd_( yuvFd )
   , outRect_( outRect )
   , bufferMs_( msToBuffer )
   , lowWater_( bufferMs_/2 )
   , highWater_( 2*bufferMs_ )
   , audioBufferBytes_( 0 )
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
   , audioOffs_( 0LL )
   , vallocCount_( 0 )
   , vfreeCount_( 0 )
   , aAllocCount_( 0 )
   , aFreeCount_( 0 )
   , vFramesQueued_( 0 )
   , vFramesPlayed_( 0 )
   , vFramesSkipped_( 0 )
   , vFramesDropped_( 0 )
   , audioPartial_( 0 )
   , prevSampleRate_( 0 )
   , prevChannels_( 0 )
   , aFramesQueued_( 0 )
   , aBytesQueued_( 0 )
   , aFramesPlayed_( 0 )
   , aFramesSkipped_( 0 )
   , aFramesDropped_( 0 )
   , firstVideoMs_( 0 )
   , lastVideoMs_( 0 )
   , firstAudioMs_( 0 )
   , lastAudioMs_( 0 )
   , firstVideoWrite_( 0 )
   , lastVideoWrite_( 0 )
   , firstAudioWrite_( 0 )
   , lastAudioWrite_( 0 )
{
debugPrint( "fds: dsp(%d), yuv(%d)\n", dspFd_, yuvFd_ );   
   videoFull_.locked_ = 0 ;
   videoFull_.header_.when_ms_ = 0 ;
   videoFull_.header_.prev_ 
      = videoFull_.header_.next_ 
      = &videoFull_.header_ ;
   videoEmpty_.locked_ = 0 ;
   videoEmpty_.header_.when_ms_ = 0 ;
   videoEmpty_.header_.prev_ 
      = videoEmpty_.header_.next_ 
      = &videoEmpty_.header_ ;
   decoderBufs_.locked_ = 0 ;
   decoderBufs_.header_.when_ms_ = 0 ;
   decoderBufs_.header_.prev_ 
      = decoderBufs_.header_.next_ 
      = &decoderBufs_.header_ ;
   audioFull_.locked_ = 0 ;
   audioFull_.header_.when_ms_ = 0 ;
   audioFull_.header_.prev_ 
      = audioFull_.header_.next_ 
      = &audioFull_.header_ ;
   audioEmpty_.locked_ = 0 ;
   audioEmpty_.header_.when_ms_ = 0 ;
   audioEmpty_.header_.prev_ 
      = audioEmpty_.header_.next_ 
      = &audioEmpty_.header_ ;

   audio_buf_info ai ;
   if( 0 == ioctl( dspFd_, SNDCTL_DSP_GETOSPACE, &ai) ){
      audioBufferBytes_ = ai.bytes ;
      debugPrint( "%llu ms/buffer @ 2 channels/44100 Hz\n",
                  audioBufferMs( 44100, 2 ) );
   }
   else
      perror( "GETOSPACE" );

#ifdef MD5OUTPUT
   MD5_Init( &videoMD5_ );
#endif
}

void mediaQueue_t::cleanup( void )
{
   cleanDecoderBufs();
   lockAndClean( decoderBufs_ );
   lockAndClean( videoFull_ );
   lockAndClean( videoEmpty_ );
   lockAndClean( audioFull_ );
   lockAndClean( audioEmpty_ );
   if( audioPartial_ ){
	   freeEntry( &audioPartial_->header_ );
	   audioPartial_ = 0 ;
   }
}

mediaQueue_t::~mediaQueue_t( void )
{
   cleanup();

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
   
bool mediaQueue_t::started( void ) const 
{ 
   return 0 != flags_ & STARTED ; 
}

void mediaQueue_t::lockAndClean( queueHeader_t &qh )
{
   queueLock_t lockVideoQ( &qh.locked_ );
   assert( lockVideoQ.weLocked() );
   cleanQueue(qh);
}

void mediaQueue_t::cleanQueue( queueHeader_t &qh )
{
   assert( qh.locked_ );
   entryHeader_t *next ;
   while( 0 != ( next = pullHead( qh.header_ ) ) ){
      freeEntry( next );
   }
}

void mediaQueue_t::flushAudio(void)
{
   if( 0 != audioPartial_ ){
      if( audioPartial_->length_ ){
         if( 0LL == firstAudioMs_ )
            firstAudioMs_ = audioPartial_->header_.when_ms_ ;
         lastAudioMs_ = audioPartial_->header_.when_ms_ ;
         audioPartial_->header_.when_ms_ += audioOffs_ ;
         queueLock_t lockAudioQ( &audioFull_.locked_, 1 );
         assert( lockAudioQ.weLocked() ); // only reader side should fail

         pushTail( audioFull_.header_, &audioPartial_->header_ );
      } // push remaining audio
      else {
         queueLock_t lockAudioQ( &audioEmpty_.locked_, 1 );
         assert( lockAudioQ.weLocked() ); // only reader side should fail

         pushTail( audioEmpty_.header_, &audioPartial_->header_ );
      } // clean up
      audioPartial_ = 0 ;
   }
}

void mediaQueue_t::resetAudio(void)
{
   if( 0 != ioctl( dspFd_, SNDCTL_DSP_RESET, 0) ){
      perror( "SNDCTL_DSP_RESET" );
   }
}

void mediaQueue_t::feedAudio
   ( unsigned char const *data, 
     unsigned             length,
     bool                 discontinuity,
     long long            when_ms ) // transport PTS
{
}


mediaQueue_t::videoEntry_t *mediaQueue_t::getPictureBuf(void)
{
   queueLock_t lockVideoQ( &videoEmpty_.locked_, 1 );
   assert( lockVideoQ.weLocked() ); // only reader side should fail

   entryHeader_t *next ;
   while( 0 != ( next = pullHead( videoEmpty_.header_ ) ) ){
      videoEntry_t *const ve = (videoEntry_t *)next ;
      if( ve->length_ == inBufferLength_ ){
debugPrint( "recycle buffer: %p\n", ve );
         ve->width_ = inWidth_ ;
         ve->height_ = inHeight_ ;
         ++vallocCount_ ;
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
   ++vallocCount_ ;

   unsigned char * const data = new unsigned char [ size ];
   videoEntry_t *const ve = (videoEntry_t *)data ;
   ve->length_ = inBufferLength_ ;
   ve->width_ = inWidth_ ;
   ve->height_ = inHeight_ ;
   ve->header_.next_ = ve->header_.prev_ = &ve->header_ ;

   return ve ;
}

mediaQueue_t::audioEntry_t *mediaQueue_t::getAudioBuf( void )
{
   {
      queueLock_t lockEmptyQ( &audioEmpty_.locked_, 1 );
      assert( lockEmptyQ.weLocked() ); // only reader side should fail
   
      entryHeader_t *next ;
      if( 0 != ( next = pullHead( audioEmpty_.header_ ) ) ){
         audioEntry_t *const ae = (audioEntry_t *)next ;
         ae->offset_ =
         ae->length_ = 
         ae->sampleRate_ =
         ae->numChannels_ = 0 ;
         ++aAllocCount_ ;
         return ae ;
debugPrint( "recycle buffer: %p\n", ae );
      }
   }

   unsigned size = sizeof(audioEntry_t)
                  -fieldsize(audioEntry_t,data_)
                  +(AUDIOSAMPLESPERFRAME*fieldsize(audioEntry_t,data_[0]));
debugPrint( "allocate buffer of size : %u\n", size );
   ++aAllocCount_ ;

   unsigned char * const data = new unsigned char [ size ];
   audioEntry_t *const ae = (audioEntry_t *)data ;
   ae->offset_ =
   ae->length_ = 
   ae->sampleRate_ =
   ae->numChannels_ = 0 ;
   ae->header_.next_ = ae->header_.prev_ = &ae->header_ ;

   return ae ;
}

long long mediaQueue_t::audioBufferMs( unsigned speed, unsigned channels )
{
   /*
    * bytes/second
    */
   unsigned bps = speed*channels*sizeof(unsigned short);
   if( 0 < bps ){
      return (audioBufferBytes_*1000)/bps ;
   }
   else {
      fprintf( stderr, "Invalid channel count or speed: %u/%u\n", channels, speed );
      return 0 ;
   }
}

void mediaQueue_t::startPlayback( void )
{
   if( 0 == ( flags_ & STARTED ) ){
      flags_ |= STARTED ;
      
      long long now = tickMs();

      { // limit scope of lock
         queueLock_t lockVideoQ( &videoFull_.locked_, 1 );
         assert( lockVideoQ.weLocked() );

         queueLock_t lockAudioQ( &audioFull_.locked_, 2 );
         assert( lockAudioQ.weLocked() );
   
         entryHeader_t *firstVideo = videoFull_.header_.next_ ;
         entryHeader_t *firstAudio = audioFull_.header_.next_ ;
   
         //
         // use latter of two timestamps (to avoid zero case)
         //
         if( firstAudio->when_ms_ < firstVideo->when_ms_ ){
            startMs_ = now - firstVideo->when_ms_ ;
         }
         else {
            startMs_ = now - firstAudio->when_ms_ ;
         }

printf( "startMs_ %lld\n", startMs_ );

         long long first = firstVideo->when_ms_ ;
         long long last = first ;
         entryHeader_t *entry = firstVideo ;
         while( entry != &videoFull_.header_ ){
            last = entry->when_ms_ ;
            entry->when_ms_ += startMs_ ;
            entry = entry->next_ ;
         }
         debugPrint( "\nvideo ms range: %llu..%llu (%lu ms)\n", first, last, (unsigned long)(last-first) );
   
         first = firstAudio->when_ms_ ;
         last = first ;
         audioOffs_ = startMs_ ;
         entry = audioFull_.header_.next_ ;
         while( entry != &audioFull_.header_ ){
            last = entry->when_ms_ ;
            entry->when_ms_ += audioOffs_ ;
            entry = entry->next_ ;
         }
         debugPrint( "audio ms range: %llu..%llu (%lu ms)\n", first, last, (unsigned long)(last-first) );

      } // scope of queue lock(s)

      playAudio(now);
   }
}

void mediaQueue_t::adjustPTS( long long startPts )
{
   if( flags_ & STARTED ){
      long long nowPlus = tickMs() + bufferMs_ - startPts ;

      startMs_ = audioOffs_ = nowPlus ;
      msOut_ = startPts ;
   }
}

void mediaQueue_t::queuePicture(videoEntry_t *ve)
{
   queueLock_t lockVideoQ( &videoFull_.locked_, 2 );
   assert( lockVideoQ.weLocked() ); // only reader side should fail

   if( 0LL == firstVideoMs_ ){
      firstVideoMs_ = ve->header_.when_ms_ ;
printf( "first video frame at %lld\n", firstVideoMs_ );
   }
   lastVideoMs_ = ve->header_.when_ms_ ;

   ve->header_.when_ms_ += startMs_ ;

   long int diff = (long)(ve->header_.when_ms_ - videoFull_.header_.next_->when_ms_ );
   if( 0 > diff )
      printf( "ms inversion: %lld/%lld (%ld)\n",
              ve->header_.when_ms_,
              videoFull_.header_.next_->when_ms_,
              diff );

   pushTail( videoFull_.header_, &ve->header_ );
   ++vFramesQueued_ ;

   assert( highWater_ms()*2 > videoTailFromNow() );
}

void mediaQueue_t::cleanDecoderBufs()
{
   queueLock_t lockEmptyQ( &videoEmpty_.locked_, 3 );
   assert( lockEmptyQ.weLocked() );

   entryHeader_t *e ;
   while( 0 != ( e = pullHead( decoderBufs_.header_ ) ) ){
      ++vfreeCount_ ;
      pushTail( videoEmpty_.header_, e );
   }
}

static char const cPicTypes[] = {
   "xIPBD"
};

void mediaQueue_t::playAudio
   ( long long when ) // in ms a.la. tickMs()
{
   if( flags_ & STARTED ){
      audio_buf_info ai ;
      if( 0 == ioctl( dspFd_, SNDCTL_DSP_GETOSPACE, &ai) ){
         unsigned bytesWritten = 0 ;
         if( 0 < ai.fragments ){
            queueLock_t lockAudioQ( &audioFull_.locked_, 3 );
            if( lockAudioQ.weLocked() ){
               while( !isEmpty( audioFull_.header_ ) ){
                  audioEntry_t *const ae = (audioEntry_t *)audioFull_.header_.next_ ;
                  long long timeDiff = ae->header_.when_ms_
                                     - when 
                                     - audioBufferMs( ae->sampleRate_, ae->numChannels_ );
                  if( 0 > timeDiff ){
                     if( ae->sampleRate_ != prevSampleRate_ ){
                        printf( "%u samples/sec\n", ae->sampleRate_ );
                        prevSampleRate_ = ae->sampleRate_ ;
                        if( 0 != ioctl( dspFd_, SNDCTL_DSP_SPEED, &prevSampleRate_ ) )
                           fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", prevSampleRate_ );
                     }
                     else if ( 0 == prevSampleRate_ ){
                        printf( "No sample rate(yet)\n" );
                     }
                     if( ae->numChannels_ != prevChannels_ ){
                        printf( "%u channels\n", ae->numChannels_ );
                        prevChannels_ = ae->numChannels_ ;
                        if( 0 != ioctl( dspFd_, SNDCTL_DSP_CHANNELS, &prevChannels_ ) )
                           fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );
                     }
                     else if ( 0 == prevChannels_ ){
                        printf( "No channel count(yet)\n" );
                     }

                     assert( ae->length_ > ae->offset_ ); // or why are we here?

                     unsigned bytes = ( ae->length_ - ae->offset_ ) * sizeof(ae->data_[0]);
                     int numWritten = write( dspFd_, ae->data_+ae->offset_, bytes );
                     if( 0 < numWritten ){
                        bytesWritten += numWritten ;
                        if( 0 == firstAudioWrite_ )
                           firstAudioWrite_ = tickMs();
                        lastAudioWrite_ = tickMs();
                        ae->offset_ += numWritten/sizeof(ae->data_[0]);

                        assert( ae->offset_ <= ae->length_ );
                        if( ae->offset_ >= ae->length_ ){
                           ++aFramesPlayed_ ;
                           entryHeader_t *const e = pullHead( audioFull_.header_ );
                           assert( e );
                           assert( e == (entryHeader_t *)ae );
                           pushTail( audioEmpty_.header_, e );
                           ++aFreeCount_ ; 
                           continue ;
                        } // finished this packet of audio
                     }
                     else if( ( -1 != numWritten ) || ( EAGAIN != errno ) ){
                        fprintf( stderr, "Short audio write: %d/%d/%d/%u/%u/%u: %m\n", dspFd_, numWritten, errno, bytes, ae->offset_, ae->length_ );
                        unsigned value ;
                        if( 0 == ioctl( dspFd_, SOUND_PCM_READ_RATE, &value ) )
                           fprintf( stderr, "speed %u\n", value );
                        else
                           perror( "PCM_READ_RATE" );
                        if( 0 == ioctl( dspFd_, SOUND_PCM_READ_CHANNELS, &value ) )
                           fprintf( stderr, "%u channels\n", value );
                        else
                           perror( "PCM_READ_CHANNELS" );
                     }
                     else {
                        int prevErr = errno ;
                        audio_buf_info ai2 ;
                        if( 0 == ioctl( dspFd_, SNDCTL_DSP_GETOSPACE, &ai2) ){
                           if( 0 < ai2.fragments ){
                              printf( "EINTR? %d/%d/%s\n", numWritten, prevErr, strerror(prevErr) );
                              continue ;
                           }
                        }
                        else
                           perror( "GETOSPACE2" );
                     }
                  } // time to play
                  else
                     debugPrint( "Not time yet: %lld/%lld, %lld\n", 
                                 when, 
                                 ae->header_.when_ms_,
                                 audioBufferMs( ae->sampleRate_, ae->numChannels_ ) );
      
                  // continue from the middle
                  break ;
               }
            }
            else {
               debugPrint( "~audioLock (%u)\n", lockAudioQ.whoLocked() );
               flags_ |= AUDIOIDLE ;
            }
         } // have some space to write
         if( ai.fragments == ai.fragstotal )
            debugPrint( "audio stall: %u of %u filled\n", bytesWritten, ai.bytes );

         if( 0 == ioctl( dspFd_, SNDCTL_DSP_GETOSPACE, &ai) ){
            if( 0 != ai.fragments ){
               flags_ |= AUDIOIDLE ;
            }
            else
               flags_ &= ~AUDIOIDLE ;
         }
         else
            perror( "GETOSPACE2" );
      }
      else
         perror( "GETOSPACE" );
   } // if we've started playback
   else
      printf( "audio not started\n" );
}

unsigned long mediaQueue_t::msVideoQueued( void ) const 
{
   entryHeader_t const * const head = videoFull_.header_.next_ ;
   entryHeader_t const * const tail = videoFull_.header_.prev_ ;
   unsigned long queueTime = (tail->when_ms_ - head->when_ms_);

   debugPrint( "%lu: %p/%p: %lld..%lld\n", queueTime, head, tail, head->when_ms_, tail->when_ms_ );
   return (unsigned long)( queueTime );
}

unsigned long mediaQueue_t::msAudioQueued( void ) const 
{
   entryHeader_t const * const head = audioFull_.header_.next_ ;
   entryHeader_t const * const tail = audioFull_.header_.prev_ ;
   unsigned long queueTime = (tail->when_ms_ - head->when_ms_);

   debugPrint( "%lu: %p/%p: %lld..%lld\n", queueTime, head, tail, head->when_ms_, tail->when_ms_ );
   return (unsigned long)( queueTime );
}

unsigned long mediaQueue_t::videoTailFromNow(void) const 
{
   unsigned long queueTime = 0 ;
   if( 0 == (flags_ & STARTED) ){
      entryHeader_t const * const tail = videoFull_.header_.prev_ ;
      if( tail != &videoFull_.header_ ){
         long delta = (tail->when_ms_ - tickMs());
         if( 0 < delta )
            queueTime = delta ;
      }
   } // clocks have been converted
   else
      queueTime = msVideoQueued();
   
   return queueTime ;
}

unsigned long mediaQueue_t::audioTailFromNow(void) const 
{
   unsigned long queueTime = 0 ;
   if( 0 == (flags_ & STARTED) ){
      entryHeader_t const * const tail = audioFull_.header_.prev_ ;
      if( tail != &audioFull_.header_ ){
         long delta = (tail->when_ms_ - tickMs());
         if( 0 < delta )
            queueTime = delta ;
      }
   } // clocks have been converted
   else
      queueTime = msAudioQueued();
   
   return queueTime ;
}

extern "C" {
   void memcpyAlign4( void *dst, void const *src, unsigned len );
};

void mediaQueue_t::playVideo( long long when )
{
   if( flags_ & STARTED ){
      queueLock_t lockVideoQ( &videoFull_.locked_, 3 );
      queueLock_t lockEmptyQ( &videoEmpty_.locked_, 4 );
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

                  ++vfreeCount_ ;
                  pushTail( videoEmpty_.header_, e );
                  ++vFramesSkipped_ ;
                  continue ;
               } // skip: more than one frame is ready
               else {
                     ++vFramesPlayed_ ;
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

                  if( 0LL == firstVideoWrite_ )
                     firstVideoWrite_ = when ;
                  lastVideoWrite_ = when ;

                  int numWritten = write( yuvFd_, ve->data_, ve->length_ );
                  if( ve->length_ == (unsigned)numWritten ){
                     entryHeader_t *const e = pullHead( videoFull_.header_ );
                     assert( e );
                     assert( e == (entryHeader_t *)ve );
                     ++vfreeCount_ ;
                     pushTail( videoEmpty_.header_, e );
                     continue ;
                  }
                  else
                     fprintf( stderr, "Short video write: %d/%d/%d/%u: %m\n", yuvFd_, numWritten, errno, ve->length_ );
               } // no peek ahead
            } // time to play
            else
               debugPrint( "idle: %llu/%llu\n", when, ve->header_.when_ms_ );

            // continue from the middle
            break ;
         }
      }
      else {
         debugPrint( "vLock: %d/%d, %u/%u\n",  
                     lockVideoQ.weLocked(), lockEmptyQ.weLocked(),
                     lockVideoQ.whoLocked(), lockEmptyQ.whoLocked()
                 );
      }
   } // if we've started playback
   else
      debugPrint( "idle\n" );
}

bool mediaQueue_t::isEmpty( void ) const {
   if( isEmpty( videoFull_.header_ )
       && 
       isEmpty( audioFull_.header_ ) ){
      audio_buf_info ai ;
      if( 0 == ioctl( dspFd_, SNDCTL_DSP_GETOSPACE, &ai) ){
         return( ai.fragments == ai.fragstotal );
      }
      else
         perror( "GETOSPACE" );
   }

   return false ;
}


// checks for end of either of two queues
void mediaQueue_t::dumpQueue( entryHeader_t const &eh,
                             entryHeader_t const &eh2 )
{
   entryHeader_t const *prev = 0 ;
   entryHeader_t const *next = eh.next_ ;
   while( ( next != &eh ) && ( next != &eh2 ) && ( next != prev ) ){
      printf( "   %p@%lld\n", next, next->when_ms_ );

      // if( next == prev ) then the node has been detached
      prev = next ;
      next = next->next_ ;
   }
}

void mediaQueue_t::dumpVideoQueue(void) const {
   printf( "---> video queue\n" ); dumpQueue( videoFull_.header_, videoEmpty_.header_ );
}

void mediaQueue_t::dumpAudioQueue(void) const {
   printf( "---> audio queue\n" ); dumpQueue( audioFull_.header_, audioEmpty_.header_ );
}

void mediaQueue_t::dumpStats(void) const 
{
   printf( "start offsets:\n"
           "   %lld video\n"
           "   %lld audio\n"
           "%lu ms of video queued (%lu from now)\n"
           "   low water mark:  %lu\n"
           "   high water mark: %lu\n"
           "   %u buffers allocated\n"
           "   %u buffers freed\n"
           "   %u video frames queued\n"
           "   %u video frames played\n"
           "   %u video frames skipped\n"
           "   %u video frames dropped\n"
           "%lu ms of audio queued (%lu from now)\n"
           "   %u audio frames queued (%u bytes)\n"
           "   %u audio frames played\n"
           "   %u audio frames skipped\n"
           "   %u audio frames dropped\n"
           "   video ms range: %lld..%lld (%lu ms)\n"
           "   video writes: %lld..%lld (%lu ms)\n"
           "   audio ms range: %lld..%lld (%lu ms)\n"
           "   audio writes: %lld..%lld (%lu ms)\n"
         ,  startMs_
         ,  audioOffs_
         ,  msVideoQueued(), videoTailFromNow()
         ,  lowWater_ms()
         ,  highWater_ms()
         ,  numAllocated()
         ,  numFreed()
         ,  vFramesQueued()
         ,  vFramesPlayed()
         ,  vFramesSkipped() 
         ,  vFramesDropped()
         ,  msAudioQueued(), audioTailFromNow()
         ,  aFramesQueued(), aBytesQueued()
         ,  aFramesPlayed()
         ,  aFramesSkipped() 
         ,  aFramesDropped()
         ,  firstVideoMs_
         ,  lastVideoMs_
         ,  (unsigned long)( lastVideoMs_ - firstVideoMs_ )
         ,  firstVideoWrite_
         ,  lastVideoWrite_
         ,  (unsigned long)( lastVideoWrite_ - firstVideoWrite_ )
         ,  firstAudioMs_
         ,  lastAudioMs_
         ,  (unsigned long)( lastAudioMs_ - firstAudioMs_ )
         ,  firstAudioWrite_
         ,  lastAudioWrite_
         ,  (unsigned long)( lastAudioWrite_ - firstAudioWrite_ )
         );
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
   if( in_synth_frame ){
      stats->inAudioSynthFrame_++ ;
      isIdle = false ;
   }
   if( in_mp3_pull ){
      stats->inAudioPull_++ ;
      isIdle = false ;
   }
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

static mediaQueue_t *inst_ = 0 ;

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
      fcntl(fdYUV, F_SETFD, FD_CLOEXEC);
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
      fcntl(fdSync, F_SETFD, FD_CLOEXEC);
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

      mediaQueue_t q( dspFd, fdYUV, 2000, outRect );
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
