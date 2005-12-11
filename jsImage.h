#ifndef __JSIMAGE_H__
#define __JSIMAGE_H__ "$Id: jsImage.h,v 1.6 2005-12-11 16:00:53 ericn Exp $"

/*
 * jsImage.h
 *
 * This header file declares the initialization routine
 * for the javaScript image routines:
 *
 *    image( {url:'string', ...} )
 *    image( cairo_surface )
 *
 * Change History : 
 *
 * $Log: jsImage.h,v $
 * Revision 1.6  2005-12-11 16:00:53  ericn
 * -partial support for cairo
 *
 * Revision 1.5  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.4  2002/11/22 15:08:01  ericn
 * -made jsImageDraw public
 *
 * Revision 1.3  2002/11/02 18:37:23  ericn
 * -made jsImageClass_ externally visible
 *
 * Revision 1.2  2002/10/15 05:00:00  ericn
 * -added imageJPEG comment
 *
 * Revision 1.1  2002/10/13 16:32:21  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "js/jsapi.h"

extern JSClass jsImageClass_ ;

//
// image.draw( x, y )
//
JSBool jsImageDraw( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval );

bool initJSImage( JSContext *cx, JSObject *glob );

#endif

