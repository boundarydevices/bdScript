#ifndef __JSTEXT_H__
#define __JSTEXT_H__ "$Id: jsText.h,v 1.1 2002-10-18 01:18:25 ericn Exp $"

/*
 * jsText.h
 *
 * This header file declares the initialization routine
 * for the text output routines:
 *
 *    jsTextAt( fontData( string ), 
 *              pointSize, 
 *              x, y, 
 *              string, 
 *              [fgColor=(RGB)( > 0xFFFFFF == default invert) ], 
 *              [bgColor=(RGB)( > 0xFFFFFF == default transparent) ], 
 *              [orientation( string: 'normal', 'inverted', 'down', 'up' )] );
 *
 * Change History : 
 *
 * $Log: jsText.h,v $
 * Revision 1.1  2002-10-18 01:18:25  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSText( JSContext *cx, JSObject *glob );

#endif

