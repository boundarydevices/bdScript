#ifndef __JSSCREEN_H__
#define __JSSCREEN_H__ "$Id: jsScreen.h,v 1.2 2002-10-20 16:30:46 ericn Exp $"

/*
 * jsScreen.h
 *
 * This header file declares the initialization for the 
 * Javascript screen-handling routines:
 *
 *    clearScreen( [rgb=0] );
 *
 *
 * Change History : 
 *
 * $Log: jsScreen.h,v $
 * Revision 1.2  2002-10-20 16:30:46  ericn
 * -modified to allow clear to specified color
 *
 * Revision 1.1  2002/10/18 01:18:25  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSScreen( JSContext *cx, JSObject *glob );

#endif

