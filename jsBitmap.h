#ifndef __JSBITMAP_H__
#define __JSBITMAP_H__ "$Id: jsBitmap.h,v 1.1 2004-03-17 04:56:19 ericn Exp $"

/*
 * jsBitmap.h
 *
 * This header file declares the initialization routine for
 * the bitmap class, which is a 2-dimensional array of bits
 * which are packed tightly (not on byte boundaries per row).
 *
 *
 * Change History : 
 *
 * $Log: jsBitmap.h,v $
 * Revision 1.1  2004-03-17 04:56:19  ericn
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


#endif

