#ifndef __JSSCREEN_H__
#define __JSSCREEN_H__ "$Id: jsScreen.h,v 1.7 2002-12-11 04:04:48 ericn Exp $"

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
 *       .invertRect( x, y, width, height );
 *       .rect( x1, y1, x2, y2, color = 0x000000 );               filled rectangle
 *       .line( x1, y1, x2, y2, penWidth = 1, color = 0x000000 ); only implemented for horizontal and vertical
 *       .box( x1, y1, x2, y2, penWidth = 1, color = 0x000000 );
 *       .buttonize( pressed, borderWidth, x1, y1, x2, y2, color );
 *       .width
 *       .height
 *
 * Change History : 
 *
 * $Log: jsScreen.h,v $
 * Revision 1.7  2002-12-11 04:04:48  ericn
 * -moved buttonize code from button to fbDev
 *
 * Revision 1.6  2002/12/04 13:13:04  ericn
 * -added rect, line, box methods
 *
 * Revision 1.5  2002/11/21 14:05:22  ericn
 * -added invertRect() method
 *
 * Revision 1.4  2002/11/02 18:36:52  ericn
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

