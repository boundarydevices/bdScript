#ifndef __JSEXIT_H__
#define __JSEXIT_H__ "$Id: jsExit.h,v 1.1 2003-07-06 01:22:23 ericn Exp $"

/*
 * jsExit.h
 *
 * This header file declares the initJSExit() routine,
 * which is used to add the exit() routine to the 
 * Javascript environment. 
 *
 * Note that the current 
 *
 *
 * Change History : 
 *
 * $Log: jsExit.h,v $
 * Revision 1.1  2003-07-06 01:22:23  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"
#include <string>

extern bool volatile exitRequested_ ;
extern int volatile  exitStatus_ ;

bool initJSExit( JSContext *cx, JSObject *glob );

#endif

