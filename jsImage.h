#ifndef __JSIMAGE_H__
#define __JSIMAGE_H__ "$Id: jsImage.h,v 1.3 2002-11-02 18:37:23 ericn Exp $"

/*
 * jsImage.h
 *
 * This header file declares the initialization routine
 * for the javaScript image routines:
 *
 *    imagePNG( data [, x, y ] )
 *    imageJPEG( data [, x, y ] )
 *
 * Change History : 
 *
 * $Log: jsImage.h,v $
 * Revision 1.3  2002-11-02 18:37:23  ericn
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

bool initJSImage( JSContext *cx, JSObject *glob );

#endif

