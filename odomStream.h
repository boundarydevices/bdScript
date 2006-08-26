#ifndef __ODOMSTREAM_H__
#define __ODOMSTREAM_H__ "$Id: odomStream.h,v 1.2 2006-08-26 16:06:15 ericn Exp $"

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
 * Revision 1.2  2006-08-26 16:06:15  ericn
 * -use new mpegQueue instead of decoder+odomVQ
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "mpegRxUDP.h"
#include "odomPlaylist.h"
#include "mpegQueue.h"
#include "fbDev.h"

class odomVideoStream_t : public mpegRxUDP_t {
public:
   odomVideoStream_t( odomPlaylist_t    &playlist, // used to get video fd
                      unsigned           port,
                      rectangle_t const &outRect );
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

   odomVideoStream_t( odomVideoStream_t const & ); // no copies
   odomPlaylist_t                 &playlist_ ;
   rectangle_t               const outRect_ ;
   mpegQueue_t                     outQueue_ ;
   unsigned                        bytesPerPicture_ ;
   long long                       startMs_ ;
   long long                       lastMs_ ;
   bool                            firstFrame_ ;
};


#endif

