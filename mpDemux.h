#ifndef __MPDEMUX_H__
#define __MPDEMUX_H__ "$Id: mpDemux.h,v 1.2 2003-07-20 18:36:05 ericn Exp $"

/*
 * mpDemux.h
 *
 * This header file declares the mpegDemux_t
 * class, which is used to demux MPEG-2 video files 
 * with associated MP3 audio.
 *
 *
 * Change History : 
 *
 * $Log: mpDemux.h,v $
 * Revision 1.2  2003-07-20 18:36:05  ericn
 * -added PTS interface
 *
 * Revision 1.1  2003/07/20 15:43:13  ericn
 * -Initial version, using libavformat
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

extern "C" {
#include "libavformat/avformat.h"
};

class mpegDemux_t {
public:
   mpegDemux_t( char const *fileName );
   ~mpegDemux_t( void );

   typedef enum frameType_e {
      audioFrame_e,
      videoFrame_e,
      endOfFile_e
   };

   frameType_e getFrame( void const   *&fData,
                         unsigned long &length,       // #bytes
                         INT64         &when_ms );    // when to play, ms relative to start

private:
   typedef enum state_e {
      end_e,
      processing_e
   };

   state_e             state_ ;
   AVPacket            packet_ ;
   AVFormatContext    *avfCtx_ ;
};

#endif

