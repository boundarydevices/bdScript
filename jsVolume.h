#ifndef __JSVOLUME_H__
#define __JSVOLUME_H__ "$Id: jsVolume.h,v 1.1 2002-11-14 13:14:47 ericn Exp $"

/*
 * jsVolume.h
 *
 * This header file declares the volume routines
 * for javascript:
 *
 *    getVolume()             - returns volume in the range [0..10]
 *    setVolume( #(0-10) )    - sets volume to specified level
 *
 * Change History : 
 *
 * $Log: jsVolume.h,v $
 * Revision 1.1  2002-11-14 13:14:47  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSVolume( JSContext *cx, JSObject *glob );

#endif

