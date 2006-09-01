#ifndef __ODOMVIDEO_H__
#define __ODOMVIDEO_H__ "$Id: odomVideo.h,v 1.3 2006-09-01 20:24:14 ericn Exp $"

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
 * Revision 1.3  2006-09-01 20:24:14  ericn
 * -keep track of PTS stats
 *
 * Revision 1.2  2006/09/01 01:02:27  ericn
 * -use mpegQueue for odomVideo
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomPlaylist.h"
#include "mpegQueue.h"
#include "mpegStream.h"

class odomVideo_t {
public:
   odomVideo_t( odomPlaylist_t    &playlist, // used to get YUV handle
                char const        *fileName,
                rectangle_t const &outRect );
   ~odomVideo_t( void );

   // returns true if ready to play
   bool initialized( void ) const { return 0 != fIn_ ; }

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
   FILE                           *fIn_ ;
   mpegStreamFile_t                stream_ ;
   mpegQueue_t                     outQueue_ ;
   odomPlaylist_t                 &playlist_ ;
   rectangle_t                     outRect_ ;
   unsigned                        bytesPerPicture_ ;
   long long                       firstPTS_ ;
   long long                       lastPTS_ ;
   long long                       start_ ;
};

#endif

