#ifndef __VIDEOFRAMES_H__
#define __VIDEOFRAMES_H__ "$Id: videoFrames.h,v 1.1 2003-07-27 15:19:12 ericn Exp $"

/*
 * videoFrames.h
 *
 * This header file declares the videoFrames_t class,
 * which is used to pull together a set of de-muxed video
 * frames, a videoQueue, an MPEG decoder, along with a PTS 
 * timestamp to provide most of the bookkeeping for synchronized 
 * video playback.
 *
 * This class is designed to be used in a loop something
 * like:
 *
 *    videoFrames_t frames( demuxFrames );
 *
 *    frames.preload();
 *
 *    frames.setStart( start_ms );
 *
 *    while( frames.pull( entry ) )
 *       display entry
 *
 * Change History : 
 *
 * $Log: videoFrames.h,v $
 * Revision 1.1  2003-07-27 15:19:12  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#ifndef __VIDEOQUEUE_H__
#include "videoQueue.h"
#endif

#ifndef __MPEGDECODE_H__
#include "mpegDecode.h"
#endif

#ifndef __MPDEMUX_H__
#include "mpDemux.h"
#endif

class videoFrames_t {
public:
   videoFrames_t( mpegDemux_t :: streamAndFrames_t const &frames,
                  unsigned maxWidth,
                  unsigned maxHeight );
   ~videoFrames_t( void );

   //
   // used to pre-buffer a set of frames at startup
   //
   bool preload( void );

   //
   // set the start playback time
   //
   inline void setStart( long long msStart ){ msStart_ = msStart ; }
   
   //
   // Pull another frame at display time, using 
   // idle time to decode additional frame(s).
   //
   // Returns false at eof
   //
   bool pull( videoQueue_t :: entry_t *&entry );

   //
   // call these only after preload()
   //
   inline unsigned short getRowStride( void ) const { return ( 0 != queue_ ) ? queue_->rowStride_ : 0 ; }
   inline unsigned short getHeight( void ) const { return ( 0 != queue_ ) ? queue_->height_ : 0 ; }

   mpegDecoder_t                           decoder_ ;
   mpegDemux_t :: streamAndFrames_t const &frames_ ;
   unsigned                         const  maxWidth_ ;
   unsigned                         const  maxHeight_ ;
   videoQueue_t                           *queue_ ;
   long long                               msStart_ ;
   long long                               lastPTS_ ;
   unsigned long                           nextInFrame_ ;
   bool                                    skippedLast_ ;
   bool                                    eof_ ;
private:
   videoFrames_t( videoFrames_t const & ); // no copies!!!
};

#endif

