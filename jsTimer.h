#ifndef __JSTIMER_H__
#define __JSTIMER_H__ "$Id: jsTimer.h,v 1.1 2002-10-27 17:42:08 ericn Exp $"

/*
 * jsTimer.h
 *
 * This header file declares the initialization routine
 * for the Javascript timer() routines:
 *
 *    timer( unsigned long ms, "handler code" );
 *    oneShot( unsigned long ms, "handler code" );
 *    cancelTimer( handle );
 *
 * Both of these routines return a handle to the timer if
 * successful, or false if an error occurs.
 *
 * Change History : 
 *
 * $Log: jsTimer.h,v $
 * Revision 1.1  2002-10-27 17:42:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSTimer( JSContext *cx, JSObject *glob );

#endif

