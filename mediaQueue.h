#ifndef __MEDIAQUEUE_H__
#define __MEDIAQUEUE_H__ "$Id: mediaQueue.h,v 1.1 2007-01-03 22:12:10 ericn Exp $"

/*
 * mediaQueue.h
 *
 * This header file declares an abstract base class
 * mediaQueue_t, which is used to represent video playback
 * using either the MPEG or XVid decoders (and possibly FFMPeg)
 * in the future.
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
 *
 * Change History : 
 *
 * $Log: mediaQueue.h,v $
 * Revision 1.1  2007-01-03 22:12:10  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbDev.h"
#ifdef MD5OUTPUT
#include <openssl/md5.h>
#endif
#include <assert.h>

class mediaQueue_t {
public:
   mediaQueue_t( int                dspFd,
                 int                yuvFd,
                 unsigned           msToBuffer,
                 rectangle_t const &outRect );
   virtual ~mediaQueue_t( void );
   void cleanup( void ); // allows memory-allocation checking before destructor

   // feeder-side interface
   virtual void feedAudio
         ( unsigned char const *data, 
           unsigned             length,
           bool                 discontinuity,
           long long            offset_ms ) = 0 ; // adjusted to real-time after buffer is full
   virtual void feedVideo
         ( unsigned char const *data, 
           unsigned             length,
           bool                 discontinuity,
           long long            offset_ms ) = 0 ; // adjusted to real-time after buffer is full
   // used by streaming when a 'new' file starts
   void flushAudio( void );
   void resetAudio( void );
   void adjustPTS( long long startPts );

   //
   // reader-side interface
   //
   //    whenMs is in real-time milliseconds a.la. tickMs()

   void playAudio( long long whenMs );
   void playVideo( long long whenMs );

   bool started( void ) const ;

   //
   // stats 
   //
   bool isEmpty( void ) const ;

   //
   // These routines return the total queue duration without
   // reference to the clock (for use while buffering)
   //
   // Because of this, they can be used before playback starts
   // and the timestamps are converted to wall time.
   //
   unsigned long msVideoQueued( void ) const ;
   unsigned long msAudioQueued( void ) const ;

   //
   // These routines return the queue duration from now until end-of-playback
   //
   unsigned long videoTailFromNow(void) const ;
   unsigned long audioTailFromNow(void) const ;

   inline unsigned long msToBuffer( void ) const { return bufferMs_ ; }
   inline unsigned long lowWater_ms( void ) const { return lowWater_ ; }
   inline unsigned long highWater_ms( void ) const { return highWater_ ; }

   inline static long long ptsToMs( long long pts ){ return pts/90 ; }

   inline unsigned numAllocated( void ) const { return vallocCount_ ; }
   inline unsigned numFreed( void ) const { return vfreeCount_ ; }

   inline unsigned vFramesQueued( void ) const { return vFramesQueued_ ; }
   inline unsigned vFramesPlayed( void ) const { return vFramesPlayed_ ; }
   inline unsigned vFramesSkipped( void ) const { return vFramesSkipped_ ; }
   inline unsigned vFramesDropped( void ) const { return vFramesDropped_ ; }

   inline unsigned aFramesQueued( void ) const { return aFramesQueued_ ; }
   inline unsigned aBytesQueued( void ) const { return aBytesQueued_ ; }
   inline unsigned aFramesPlayed( void ) const { return aFramesPlayed_ ; }
   inline unsigned aFramesSkipped( void ) const { return aFramesSkipped_ ; }
   inline unsigned aFramesDropped( void ) const { return aFramesDropped_ ; }

   inline long long firstVideoWrite( void ) const { return firstVideoWrite_ ; }
   inline long long lastVideoWrite( void ) const { return lastVideoWrite_ ; }

   inline long long firstAudioWrite( void ) const { return firstAudioWrite_ ; }
   inline long long lastAudioWrite( void ) const { return lastAudioWrite_ ; }

   void dumpStats(void) const ;
   void dumpVideoQueue(void) const ;
   void dumpAudioQueue(void) const ;

   void startPlayback( void );

protected:
   mediaQueue_t( mediaQueue_t const & );
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
   static void dumpQueue( entryHeader_t const &eh1,
                          entryHeader_t const &eh2 );

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
   void cleanDecoderBufs();

   struct audioEntry_t {
      entryHeader_t  header_ ;
      unsigned       offset_ ; // next sample to read (in samples)
      unsigned       length_ ; // number of samples filled
      unsigned       sampleRate_ ;
      unsigned       numChannels_ ;
      unsigned short data_[1];
   };

   audioEntry_t *getAudioBuf();

   //
   // how much time does the audio queue take at a given playback rate?
   //
   long long audioBufferMs( unsigned speed, unsigned channels );

   int const  dspFd_ ;
   int const  yuvFd_ ;

   rectangle_t const outRect_ ;
   
   unsigned long const bufferMs_ ;
   unsigned long const lowWater_ ;
   unsigned long const highWater_ ;

   unsigned long       audioBufferBytes_ ;

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
   long long     audioOffs_ ;

   queueHeader_t videoFull_ ;
   queueHeader_t videoEmpty_ ;
   queueHeader_t decoderBufs_ ;

   queueHeader_t audioFull_ ;
   queueHeader_t audioEmpty_ ;

   unsigned      vallocCount_ ;
   unsigned      vfreeCount_ ;

   unsigned      aAllocCount_ ;
   unsigned      aFreeCount_ ;

   unsigned      vFramesQueued_ ;
   unsigned      vFramesPlayed_ ;
   unsigned      vFramesSkipped_ ;
   unsigned      vFramesDropped_ ;
#ifdef MD5OUTPUT
   MD5_CTX       videoMD5_ ;
#endif

   audioEntry_t *audioPartial_ ; // contains partial data (if not NULL)
   unsigned      prevSampleRate_ ;
   unsigned      prevChannels_ ;

   unsigned      aFramesQueued_ ;
   unsigned      aBytesQueued_ ;
   unsigned      aFramesPlayed_ ;
   unsigned      aFramesSkipped_ ;
   unsigned      aFramesDropped_ ;
   friend class queueLock_t ;

   long long     firstVideoMs_ ;
   long long     lastVideoMs_ ;
   long long     firstAudioMs_ ;
   long long     lastAudioMs_ ;

   long long     firstVideoWrite_ ;
   long long     lastVideoWrite_ ;

   long long     firstAudioWrite_ ;
   long long     lastAudioWrite_ ;
};

inline bool mediaQueue_t::isEmpty( entryHeader_t const &eh )
{
   return ( eh.prev_ == eh.next_ )
          &&
          ( eh.prev_ == &eh );
}
   
void mediaQueue_t::unlink( entryHeader_t &entry )
{
   assert( entry.next_ && ( entry.next_ != &entry ) );
   assert( entry.prev_ && ( entry.prev_ != &entry ) );
   entry.next_->prev_ = entry.prev_ ;
   entry.prev_->next_ = entry.next_ ;

   entry.next_ = entry.prev_ = &entry ;
}

mediaQueue_t::entryHeader_t *mediaQueue_t::pullHead( entryHeader_t &eh )
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

void mediaQueue_t::pushTail( entryHeader_t &head, 
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

void mediaQueue_t::freeEntry( entryHeader_t *e )
{
   assert( e && ( e->next_ == e ) && ( e->prev_ == e ) );
   delete [] (char *)e ;
}
   
class queueLock_t {
public:
   queueLock_t( unsigned volatile *lock, unsigned id = 1 );
   ~queueLock_t( void );

   bool weLocked( void ) const { return 0 == prevVal_ ; }
   bool wasLocked( void ) const { return 0 != prevVal_ ; }
   unsigned whoLocked( void ) const { return prevVal_ ; }

private:
   queueLock_t( queueLock_t const & );
   unsigned volatile *lock_ ;
   unsigned           id_ ;
   unsigned           prevVal_ ;
};


#endif

