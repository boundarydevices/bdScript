#ifndef __JSALPHAMAP_H__
#define __JSALPHAMAP_H__ "$Id: jsAlphaMap.h,v 1.2 2004-03-17 04:56:19 ericn Exp $"

/*
 * jsAlphaMap.h
 *
 * This header file declares the Javascript alpha
 * map structures for use in rendering text and 
 * graphics primitives.
 *
 *
 * Change History : 
 *
 * $Log: jsAlphaMap.h,v $
 * Revision 1.2  2004-03-17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.1  2002/11/02 18:39:42  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

extern JSClass jsAlphaMapClass_ ;

enum jsAlphaMap_tinyId {
   ALPHAMAP_WIDTH, 
   ALPHAMAP_HEIGHT, 
   ALPHAMAP_PIXBUF,
   ALPHAMAP_NUMPROPERTIES
};

bool initJSAlphaMap( JSContext *cx, JSObject *glob );

#endif

