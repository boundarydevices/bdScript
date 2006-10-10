#ifndef __ODOMVIDEO_H__
#define __ODOMVIDEO_H__ "$Id: odomVideo.h,v 1.6 2006-10-10 20:50:13 ericn Exp $"

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
 * Revision 1.6  2006-10-10 20:50:13  ericn
 * -use fds, not playlist
 *
 * Revision 1.5  2006/09/17 15:53:54  ericn
 * -use pipeline for mpeg file reads
 *
 * Revision 1.4  2006/09/04 16:42:14  ericn
 * -add audio playback call
 *
 * Revision 1.3  2006/09/01 20:24:14  ericn
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


#include "mpegQueue.h"
#include "mpegStream.h"

class odomVideo_t {
public:
   odomVideo_t( int                fdDsp,
                int                fdYUV,
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

   // called from audio avail signal to fill audio buffer
   void doAudio( void );

   void dump( void );

private:
   odomVideo_t( odomVideo_t const & );
   char                            cmdLine_[512];
   FILE                           *fIn_ ;
   mpegStreamFile_t                stream_ ;
   mpegQueue_t                     outQueue_ ;
   rectangle_t                     outRect_ ;
   unsigned                        bytesPerPicture_ ;
   long long                       firstPTS_ ;
   long long                       lastPTS_ ;
   long long                       start_ ;
};

#endif

