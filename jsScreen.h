#ifndef __JSSCREEN_H__
#define __JSSCREEN_H__ "$Id: jsScreen.h,v 1.1 2002-10-18 01:18:25 ericn Exp $"

/*
 * jsScreen.h
 *
 * This header file declares the initialization for the 
 * Javascript screen-handling routines:
 *
 *    clearScreen();
 *
 *
 * Change History : 
 *
 * $Log: jsScreen.h,v $
 * Revision 1.1  2002-10-18 01:18:25  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSScreen( JSContext *cx, JSObject *glob );

#endif

