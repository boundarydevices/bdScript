#ifndef __JSSCREEN_H__
#define __JSSCREEN_H__ "$Id: jsScreen.h,v 1.4 2002-11-02 18:36:52 ericn Exp $"

/*
 * jsScreen.h
 *
 * This header file declares the initialization for the 
 * Javascript screen object, properties, and methods :
 *
 *    screen
 *       .clear( [rgb=0] );
 *       .getPixel( x, y );
 *       .setPixel( x, y, rgb );
 *       .getRect( x, y, width, height );
 *       .width
 *       .height
 *
 * Change History : 
 *
 * $Log: jsScreen.h,v $
 * Revision 1.4  2002-11-02 18:36:52  ericn
 * -added getPixel, setPixel, getRect
 *
 * Revision 1.3  2002/10/31 02:08:07  ericn
 * -made screen object, got rid of bare clearScreen method
 *
 * Revision 1.2  2002/10/20 16:30:46  ericn
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

