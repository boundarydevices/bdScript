#ifndef __JSBITMAP_H__
#define __JSBITMAP_H__ "$Id: jsBitmap.h,v 1.2 2004-07-04 21:34:11 ericn Exp $"

/*
 * jsBitmap.h
 *
 * This header file declares the initialization routine for
 * the bitmap class, which is a 2-dimensional array of bits
 * packed on longword boundaries.
 *
 * Bits are addressed from 0 (high bit) to 7 (low bit) within
 * each byte.
 *
 * Change History : 
 *
 * $Log: jsBitmap.h,v $
 * Revision 1.2  2004-07-04 21:34:11  ericn
 * -expose conversions
 *
 * Revision 1.1  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include "js/jsapi.h"

extern JSClass jsBitmapClass_ ;

enum jsBitmap_tinyId {
   BITMAP_WIDTH, 
   BITMAP_HEIGHT, 
   BITMAP_PIXBUF,
   BITMAP_NUMPROPERTIES
};

bool initJSBitmap( JSContext *cx, JSObject *glob );

//
// draw an alpha map into a bitmap at specified
// location
//
void alphaMapToBitmap( JSContext *cx, 
                       JSObject  *alphaMap,
                       JSObject  *bitmap,
                       unsigned   x,
                       unsigned   y );

//
// draw (dither) an image into a bitmap at specified
// location
//
void imageToBitmap( JSContext *cx, 
                    JSObject  *image,      // input
                    JSObject  *bitmap,     // output
                    unsigned   x,
                    unsigned   y );

#endif

