#ifndef __JSALPHAMAP_H__
#define __JSALPHAMAP_H__ "$Id: jsAlphaMap.h,v 1.1 2002-11-02 18:39:42 ericn Exp $"

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
 * Revision 1.1  2002-11-02 18:39:42  ericn
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

extern JSPropertySpec alphaMapProperties_[ALPHAMAP_NUMPROPERTIES+1]; // NULL-terminated

bool initJSAlphaMap( JSContext *cx, JSObject *glob );

#endif

