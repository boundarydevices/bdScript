#ifndef __JSTOUCH_H__
#define __JSTOUCH_H__ "$Id: jsTouch.h,v 1.1 2002-11-03 15:39:36 ericn Exp $"

/*
 * jsTouch.h
 *
 * This header file declares the Javascript Touch
 * Screen initialization routine.
 *
 * Internally, the touch-screen routines are:
 *
 *    onTouch( "code to execute" );
 *    onRelease( "code to execute" );
 *
 *    getTouchX();
 *    getTouchY();
 * 
 * Change History : 
 *
 * $Log: jsTouch.h,v $
 * Revision 1.1  2002-11-03 15:39:36  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

#ifndef __TSTHREAD_H__
#include "tsThread.h"
#endif

//
// returns a pointer to the touch-screen thread.
// 
// delete'ing it will shut the thread down.
//
bool initJSTouch( touchScreenThread_t *&thread,
                  JSContext            *cx, 
                  JSObject             *glob );


#endif

