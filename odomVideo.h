#ifndef __ODOMVIDEO_H__
#define __ODOMVIDEO_H__ "$Id: odomVideo.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * odomVideo.h
 *
 * This header file declares the odomVideo_t class, which
 * is used to keep track of the details for playback of a
 * single MPEG within an odometer application.
 *
 *
 * Change History : 
 *
 * $Log: odomVideo.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomPlaylist.h"
#include "odomVQ.h"
#include "memFile.h"
#include "mpDemux.h"
#include "mpegDecode.h"

class odomVideo_t {
public:
   odomVideo_t( odomPlaylist_t &playlist, // used to get YUV handle
                char const     *fileName,
                unsigned        outx = 0,
                unsigned        outy = 0,
                unsigned        outw = 0,
                unsigned        outh = 0 );
   ~odomVideo_t( void );

   // returns true if ready to play
   bool initialized( void ) const { return 0 != vStream_ ; }

   // start playback (opens YUV) - returns true if successful
   bool startPlayback( void );

   // called repeatedly to decode frames to fill the output queue
   // returns false when complete
   bool playback( void );

   bool completed( void );

   // called to pump frame(s) to frame buffer from vsync
   void doOutput( void );

   void dump( void );

private:
   odomVideo_t( odomVideo_t const & );
   memFile_t                       fIn_ ;
   odomPlaylist_t                 &playlist_ ;
   mpegDecoder_t                   decoder_ ;
   odometerVideoQueue_t            outQueue_ ;
   mpegDemux_t::bulkInfo_t const  *bi_ ;
   mpegDemux_t::streamAndFrames_t *vStream_ ;
   mpegDemux_t::frame_t const     *next_ ;
   mpegDemux_t::frame_t const     *end_ ;
   unsigned                        outX_ ;
   unsigned                        outY_ ;
   unsigned                        outW_ ;
   unsigned                        outH_ ;
   unsigned                        bytesPerPicture_ ;
   long long                       start_ ;
};

#endif
