#ifndef __ODOMSTREAM_H__
#define __ODOMSTREAM_H__ "$Id: odomStream.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * odomStream.h
 *
 * This header file declares the odomVideoStream_t
 * class, which builds upon mpegRxUDP_t by adding 
 * a video decoder and output queue.
 *
 *
 * Change History : 
 *
 * $Log: odomStream.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "mpegRxUDP.h"
#include "odomPlaylist.h"
#include "odomVQ.h"
#include "mpegDecode.h"

class odomVideoStream_t : public mpegRxUDP_t {
public:
   odomVideoStream_t( odomPlaylist_t &playlist, // used to get video fd
                      unsigned        port,
                      unsigned        outx = 0,
                      unsigned        outy = 0,
                      unsigned        outw = 0,
                      unsigned        outh = 0 );
   virtual ~odomVideoStream_t( void );

   virtual void onNewFile( char const *fileName,
                           unsigned    fileNameLen );
   virtual void onRx( bool                 isVideo,
                      bool                 discont,
                      unsigned char const *fData,
                      unsigned             length,
                      long long            pts,
                      long long            dts );
   virtual void onEOF( char const   *fileName,
                       unsigned long totalBytes,
                       unsigned long videoBytes,
                       unsigned long audioBytes );

   // called to pump frame(s) to frame buffer from vsync
   void doOutput( void );

   void dump( void );

private:
   enum state_e {
      INIT       = 0,
      WAITIFRAME = 1,
      PLAYBACK   = 2
   };

   odomVideoStream_t( odomVideoStream_t const & );
   odomPlaylist_t                 &playlist_ ;
   mpegDecoder_t                   decoder_ ;
   odometerVideoQueue_t            outQueue_ ;
   unsigned                        outX_ ;
   unsigned                        outY_ ;
   unsigned                        outW_ ;
   unsigned                        outH_ ;
   unsigned                        bytesPerPicture_ ;
   long long                       start_ ;
   state_e                         state_ ;
};


#endif

