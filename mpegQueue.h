#ifndef __MPEGQUEUE_H__
#define __MPEGQUEUE_H__ "$Id: mpegQueue.h,v 1.13 2007-01-03 22:07:19 ericn Exp $"

/*
 * mpegQueue.h
 *
 * This header file declares the mpegQueue_t (libmpeg2) derivative
 * of the mediaQueue_t class declared in mediaQueue.h
 *
 * Change History : 
 *
 * $Log: mpegQueue.h,v $
 * Revision 1.13  2007-01-03 22:07:19  ericn
 * -additional level of indirection (mediaQueue_t)
 *
 * Revision 1.12  2006/10/16 22:29:48  ericn
 * -added aBytesQueued_ member, method
 *
 * Revision 1.11  2006/09/12 13:40:30  ericn
 * -add resetAudio method
 *
 * Revision 1.10  2006/09/06 15:09:12  ericn
 * -add utility routines for use in streaming
 *
 * Revision 1.9  2006/09/05 02:29:03  ericn
 * -added queue dumping utilities for debug
 *
 * Revision 1.8  2006/09/04 16:43:40  ericn
 * -use audio timing
 *
 * Revision 1.7  2006/09/04 14:34:16  ericn
 * -add audio support
 *
 * Revision 1.6  2006/09/01 22:52:16  ericn
 * -change to finer granularity for buffer sizes (er..time)
 *
 * Revision 1.5  2006/09/01 00:49:40  ericn
 * -change names to lowWater_ms() and highWater_ms()
 *
 * Revision 1.4  2006/08/30 02:10:50  ericn
 * -add statistics
 *
 * Revision 1.3  2006/08/26 16:07:39  ericn
 * -move inlines into .cpp module, add output rect
 *
 * Revision 1.2  2006/08/25 14:53:48  ericn
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

#include "mediaQueue.h"
#include "madDecode.h"

class mpegQueue_t : public mediaQueue_t {
public:
   mpegQueue_t( int                dspFd,
                int                yuvFd,
                unsigned           msToBuffer,
                rectangle_t const &outRect );
   ~mpegQueue_t( void );
   
   // feeder-side interface
   void feedAudio
         ( unsigned char const *data, 
           unsigned             length,
           bool                 discontinuity,
           long long            offset_ms ); // adjusted to real-time after buffer is full

   void feedVideo
         ( unsigned char const *data, 
           unsigned             length,
           bool                 discontinuity,
           long long            offset_ms ); // adjusted to real-time after buffer is full
   
   void addDecoderBuf();
private:
   mpegQueue_t( mpegQueue_t const & );
   mpeg2dec_t   *decoder_ ;
   madDecoder_t  audioDecoder_ ;
};

#endif

