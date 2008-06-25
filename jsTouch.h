#ifndef __JSTOUCH_H__
#define __JSTOUCH_H__ "$Id: jsTouch.h,v 1.6 2008-06-25 01:19:38 ericn Exp $"

/*
 * jsTouch.h
 *
 * This header file declares the Javascript Touch
 * Screen initialization routine.
 *
 * The touch-screen api includes three global functions:
 *
 *    onTouch( "code to execute" );
 *    onRelease( "code to execute" );
 *    onMove( "code to execute" );
 *
 * and a 'touchScreen' object, which has the following
 * methods:
 *
 *    getX()      - returns x-position of the last touch
 *    getY()      - returns y-position of the last touch
 *
 * Change History : 
 *
 * $Log: jsTouch.h,v $
 * Revision 1.6  2008-06-25 01:19:38  ericn
 * add real mouse support (Davinci only)
 *
 * Revision 1.5  2004/12/28 03:35:09  ericn
 * -added shutdown routine
 *
 * Revision 1.4  2004/11/16 04:06:25  ericn
 * -Put it back
 *
 * Revision 1.2  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.1  2002/11/03 15:39:36  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSTouch( JSContext            *cx, 
                  JSObject             *glob );

void shutdownTouch( void );
void onRelease( void );

#endif

