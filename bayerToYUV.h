#ifndef __BAYERTOYUV_H__
#define __BAYERTOYUV_H__ "$Id$"

/*
 * bayerToYUV.h
 *
 * This header file declares the bayerToYUV() routine to 
 * convert Bayer format data to planar YUV data:
 *
 *	from :	    GRGRGRGR		to:   YYYYYYYY
 *		    BGBGBGBG                  YYYYYYYY       UUUU       VVVV 
 *                  GRGRGRGR                  YYYYYYYY       UUUU       VVVV 
 *                  BGBGBGBG                  YYYYYYYY
 *
 * Copyright Boundary Devices, Inc. 2010
 */

extern void bayerToYUV
	( unsigned char const *bayerIn, 
	  unsigned inWidth,
	  unsigned inHeight,
	  unsigned inStride,
	  unsigned char *yOut,
	  unsigned char *uOut,
	  unsigned char *vOut,
	  unsigned	 yStride,
	  unsigned	 uvStride,
	  unsigned char *rgbOut = 0 );

#endif

