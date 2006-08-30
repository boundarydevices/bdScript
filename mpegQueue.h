#ifndef __MPEGQUEUE_H__
#define __MPEGQUEUE_H__ "$Id: mpegQueue.h,v 1.4 2006-08-30 02:10:50 ericn Exp $"

/*
 * mpegQueue.h
 *
 * This header file declares the mpegQueue_t
 * class, which encapsulates parsing of MPEG 1/2
 * audio and video for file-based or streaming
 * video.
 *
 * As you might expect, there are two primary interfaces:
 *
 * The feeder:
 *
 *    feed( pts, data )
 *
 * The reader:
 *    playAudio()
 *    playVideo()
 *
 * Note that this class supports read during feed so that 
 * the reading can occur from a vertical sync signal handler
 * or audio ready signal handler. This is accomplished by
 * the feed() side setting 'busy' flags that are checked
 * by the signal handlers.
 *
 * In the video case (vsync handler):
 *    if the feed() side is currently manipulating the video 
 *    output queue, the playVideo() routine will simply do nothing
 *    and the frame will be processed the next time around. 
 *
 *    This works because the vsync signal is periodic
 *
 * In the audio case (audio queue ready signal):
 *    if the feed() side is currently manipulating the audio output queue, 
 *    the playAudio() routine will return NULL, but set a flag indicating 
 *    that playback has not yet started. The feed() side will check this 
 *    flag on its' way out of the queueing routine and pump the data to the
 *    audio driver (if started).
 * 
 * Change History : 
 *
 * $Log: mpegQueue.h,v $
 * Revision 1.4  2006-08-30 02:10:50  ericn
 * -add statistics
 *
 * Revision 1.3  2006/08/26 16:07:39  ericn
 * -move inlines into .cpp module, add output rect
 *
 * Revision 1.2  2006/08/25 14:53:48  ericn
 * -add timing support, queuing info methods
 *
 * Revision 1.1  2006/08/25 00:29:45  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


extern "C" {
#include <inttypes.h>
#include <mpeg2dec/mpeg2.h>
};


#ifdef MD5OUTPUT
#undef MD5OUTPUT
#endif

#ifdef MD5OUTPUT
#include <openssl/md5.h>
#endif

#include "fbDev.h"

class mpegQueue_t {
public:
   mpegQueue_t( int                dspFd,
                int                yuvFd,
                unsigned           secondsToBuffer,
                rectangle_t const &outRect );
   ~mpegQueue_t( void );
   void cleanup( void ); // allows memory-allocation checking before destructor

   // feeder-side interface
   void feedAudio
         ( unsigned char const *data, 
           unsigned             length,
           bool                 discontinuity,
           long long            offset_ms ); // adjusted to real-time after buffer is full
   void feedVideo
         ( unsigned char const *data, 
           unsigned             length,
           bool                 discontinuity,
           long long            offset_ms ); // adjusted to real-time after buffer is full
   // used by streaming when a 'new' file starts
   void adjustPTS( long long startPts );


   //
   // reader-side interface
   //
   //    whenMs is in real-time milliseconds a.la. tickMs()

   void playAudio( long long whenMs );
   void playVideo( long long whenMs );

   //
   // stats 
   //
   unsigned long msVideoQueued( void ) const ;
   inline unsigned long msToBuffer( void ) const { return bufferMs_ ; }
   inline unsigned long msHalfBuffer( void ) const { return halfBuffer_ ; }
   inline unsigned long msDoubleBuffer( void ) const { return doubleBuffer_ ; }

   inline static long long ptsToMs( long long pts ){ return pts/90 ; }

   inline unsigned numAllocated( void ) const { return allocCount_ ; }
   inline unsigned numFreed( void ) const { return freeCount_ ; }

   inline unsigned vFramesQueued( void ) const { return vFramesQueued_ ; }
   inline unsigned vFramesPlayed( void ) const { return vFramesPlayed_ ; }
   inline unsigned vFramesSkipped( void ) const { return vFramesSkipped_ ; }

private:
   mpegQueue_t( mpegQueue_t const & );
   enum flags_e {
      STARTED     = 1
   ,  AUDIOIDLE   = 2
   ,  NEEDIFRAME  = 4
   };

   struct entryHeader_t {
      entryHeader_t  *next_ ;
      entryHeader_t  *prev_ ;
      long long      when_ms_ ;
   };

   inline static bool isEmpty( entryHeader_t const & );
   inline static entryHeader_t *pullHead( entryHeader_t & );
   inline static void pushTail( entryHeader_t &head, entryHeader_t *entry );
   inline static void unlink( entryHeader_t &entry );
   inline static void freeEntry( entryHeader_t *e );

   struct queueHeader_t {
      entryHeader_t  header_ ;
      unsigned       locked_ ;
   };
   void cleanQueue( queueHeader_t &qh );
   void lockAndClean( queueHeader_t &qh );

   struct videoEntry_t {
      entryHeader_t  header_ ;
      unsigned       length_ ;
      unsigned short width_ ;
      unsigned short height_ ;
      unsigned char  data_[1];
   };

   videoEntry_t *getPictureBuf();
   void queuePicture(videoEntry_t *);
   void addDecoderBuf();
   void cleanDecoderBufs();

   void startPlayback( void );

   struct audioEntry_t {
      entryHeader_t  header_ ;
      unsigned       offset_ ;
      unsigned       length_ ;
      unsigned char  data_[1];
   };

   int const  dspFd_ ;
   int const  yuvFd_ ;

   rectangle_t const outRect_ ;
   
   unsigned long const bufferMs_ ;
   unsigned long const halfBuffer_ ;
   unsigned long const doubleBuffer_ ;

   unsigned long flags_ ;

   unsigned      prevOutWidth_ ;
   unsigned      prevOutHeight_ ;
   unsigned      prevOutBufferLength_ ;
   unsigned     *yuvOut_ ;

   unsigned      inWidth_ ;
   unsigned      inHeight_ ;
   unsigned      inStride_ ;
   unsigned      inBufferLength_ ;

   unsigned      inRate_ ;
   long long     msPerPic_ ;
   long long     msOut_ ;
   long long     startMs_ ;

   queueHeader_t videoFull_ ;
   queueHeader_t videoEmpty_ ;
   queueHeader_t decoderBufs_ ;

   queueHeader_t audioFull_ ;
   queueHeader_t audioEmpty_ ;

   unsigned      allocCount_ ;
   unsigned      freeCount_ ;

   mpeg2dec_t * const decoder_ ;

   unsigned      vFramesQueued_ ;
   unsigned      vFramesPlayed_ ;
   unsigned      vFramesSkipped_ ;
#ifdef MD5OUTPUT
   MD5_CTX       videoMD5_ ;
#endif

   friend class queueLock_t ;
};


#endif

