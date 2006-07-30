#ifndef __MPEGYUV_H__
#define __MPEGYUV_H__ "$Id: mpegYUV.h,v 1.2 2006-07-30 21:35:42 ericn Exp $"

/*
 * mpegYUV.h
 *
 * This header file declares the mpegYUV_t class, 
 * which is used mostly to keep track of the state
 * of an MPEG decoder in a YUV, state-driven player.
 *
 * General operation should follow the flow:
 *
 *    construct
 *    feed and parse until header read
 *    construct and initialize output device
 *
 *       parse and pull pictures until need data
 *       feed data
 *
 * Change History : 
 *
 * $Log: mpegYUV.h,v $
 * Revision 1.2  2006-07-30 21:35:42  ericn
 * -compile under latest libmpeg2
 *
 * Revision 1.1  2005/04/24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include <linux/sm501yuv.h>
extern "C" {
#include <inttypes.h>
#include <mpeg2dec/mpeg2.h>
#include <sys/mman.h>
};
#include "tickMs.h"

class mpegYUV_t {
public:
   mpegYUV_t( void );
   ~mpegYUV_t( void );

   enum state_t {
      needData,
      needParse,
      header,
      picture
   };

   void feed( void const *inData, 
              unsigned    numBytes,
              long long   pts );

   inline state_t getState( void ) const { return state_ ; }
   state_t parse( void );

   //
   // These can be called after the first header
   //
   inline bool haveHeader( void ) const { return 0 != sequence_.width ; } 
   inline unsigned short width( void ) const { return sequence_.width ; }
   inline unsigned short stride( void ) const { return stride_ ; }
   inline unsigned short height( void ) const { return sequence_.height ; }
   inline unsigned frameRate( void ) const { return frameRate_ ; }
   inline unsigned ptsPerFrame( void ) const { return ptsPerFrame_ ; }

   //
   // These calls are only valid after a picture is returned
   //

   // returned in libmpeg yuv form, must be interleaved
   inline mpeg2_picture_t const &getPicture( void ) const { return *mpegInfo_->display_picture ; }
   inline mpeg2_fbuf_t const &getDisplayBuf( void ) const { return *mpegInfo_->display_fbuf ; }
   void yuvBufs( unsigned char *&yBuf,
                 unsigned char *&uBuf,
                 unsigned char *&vBuf ) const ;
   inline long long relativePTS( void ) const { return mpegInfo_->display_picture->temporal_reference - firstPts_ ; }


   //
   // --- internals, don't look !
   //
   void *getPictureBuf( unsigned char **planes,
                        unsigned        height,
                        unsigned        stride );

   struct buf_t {
      buf_t *next_ ;
      char  *data_ ;
   };

   state_t           state_ ;
   long long         firstPts_ ;
   long long         iframePts_ ;
   long long         pts_ ;
   mpeg2dec_t       *decoder_ ;
   mpeg2_info_t     *mpegInfo_ ;
   mpeg2_sequence_t  sequence_ ;
   unsigned          stride_ ;
   unsigned          frameRate_ ;
   unsigned          ptsPerFrame_ ;
   buf_t            *bufs_ ;
   unsigned char     feedBuf_[4096];
};


#endif

