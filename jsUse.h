#ifndef __JSUSE_H__
#define __JSUSE_H__ "$Id: jsUse.h,v 1.1 2003-01-20 06:23:59 ericn Exp $"

/*
 * jsUse.h
 *
 * This header file declares the initialization routine for the
 * use() directive, which loads a javascript module and returns
 * its' handle.
 *
 * Usage is:
 *
 *    var module = use( "relativeOrAbsolueURL.js" );
 *
 * Change History : 
 *
 * $Log: jsUse.h,v $
 * Revision 1.1  2003-01-20 06:23:59  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSUse( JSContext *cx, JSObject *glob );

#endif

