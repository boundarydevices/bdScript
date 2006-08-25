#ifndef __MPEGQUEUE_H__
#define __MPEGQUEUE_H__ "$Id: mpegQueue.h,v 1.2 2006-08-25 14:53:48 ericn Exp $"

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
 * Revision 1.2  2006-08-25 14:53:48  ericn
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

class mpegQueue_t {
public:
   mpegQueue_t( int      dspFd,
                int      yuvFd,
                unsigned secondsToBuffer );
   ~mpegQueue_t( void );

   // feeder-side interface
   void feedAudio
         ( unsigned char const *data, 
           unsigned             length,
           bool                 discontinuity,
           long long            pts ); // transport PTS
   void feedVideo
         ( unsigned char const *data, 
           unsigned             length,
           bool                 discontinuity,
           long long            pts ); // transport PTS

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

   void startPlayback( void );

   struct audioEntry_t {
      entryHeader_t  header_ ;
      unsigned       offset_ ;
      unsigned       length_ ;
      unsigned char  data_[1];
   };

   int const dspFd_ ;
   int const yuvFd_ ;
   
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
   long long     ptsOut_ ;
   long long     startPts_ ;

   queueHeader_t videoFull_ ;
   queueHeader_t videoEmpty_ ;

   queueHeader_t audioFull_ ;
   queueHeader_t audioEmpty_ ;

   mpeg2dec_t * const decoder_ ;

#ifdef MD5OUTPUT
   MD5_CTX       videoMD5_ ;
#endif

   friend class queueLock_t ;
};


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
   entry->prev_ = head.prev_;
   entry->next_ = &head ;
   head.prev_->next_ = entry ;
   head.prev_ = entry ;
}

void mpegQueue_t::freeEntry( entryHeader_t *e )
{
   delete [] (char *)e ;
}

#endif

