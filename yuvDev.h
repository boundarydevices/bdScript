#ifndef __YUVDEV_H__
#define __YUVDEV_H__ "$Id: yuvDev.h,v 1.1 2005-04-24 18:55:03 ericn Exp $"

/*
 * yuvDev.h
 *
 * This header file declares the yuvDev_t class, 
 * which is used mostly to wrap the various ioctl
 * calls provided by the sm501yuv driver.
 *
 * Note that usage of this class is stateful. 
 * The following order must be followed:
 *
 *    1. Construct the object
 *    2. Call initHeader with height, width, ...
 *    3. Calls to getBuf() and write() with yuv data from mpegYUV_t
 *
 * A yuvDev_t object may be poll'd for write space
 * to wait for outbound space.
 *
 * Change History : 
 *
 * $Log: yuvDev.h,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */

extern "C" {
#include <linux/sm501yuv.h>
#include <linux/videodev.h>
};

#define VIDEO_MAX_FRAME		32

class yuvDev_t {
public:
   yuvDev_t( char const *devName = "/dev/yuv" );
   ~yuvDev_t( void );

   inline bool isOpen( void ) const { return 0 <= fd_ ; }
   inline int  getFd( void ) const { return fd_ ; }

   void initHeader( unsigned xPos, 
                    unsigned yPos,
                    unsigned inWidth,
                    unsigned inHeight,
                    unsigned outWidth,
                    unsigned outHeight );
   inline bool haveHeaders( void ) const { return 0 < vmm_.frames ; }
   inline unsigned numEntries( void ) const { return vmm_.frames ; }
   inline void *getEntry( unsigned idx ) const { return ( idx < numEntries() ) ? frames_[idx] : 0 ; }
   inline unsigned long getEntrySize( void ) const { return haveHeaders() ? entrySize_ : 0 ; }
   inline unsigned getInWidth( void ) const { return haveHeaders() ? inWidth_ : 0 ; }
   inline unsigned getRowStride( void ) const { return haveHeaders() ? ((inWidth_+15)/16)<<4 : 0 ; }
   inline unsigned getInHeight( void ) const { return haveHeaders() ? inHeight_ : 0 ; }

   void *getBuf( unsigned &index );
   bool write( unsigned index, long long whenMs );

   int               fd_ ;
   sm501yuvPlane_t   plane_ ;
   struct video_mbuf vmm_ ;
   unsigned          inWidth_ ;
   unsigned          inHeight_ ;
   unsigned          entrySize_ ;
   void             *mappedMem_ ;
   void            **frames_ ; // by index
};


#endif

