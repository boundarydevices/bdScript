#ifndef __JSCAMERA_H__
#define __JSCAMERA_H__ "$Id: jsCamera.h,v 1.1 2003-04-21 00:25:19 ericn Exp $"

/*
 * jsCamera.h
 *
 * This header file declares the initialization
 * routines for the Camera class.
 *
 *    Constructor Camera( deviceName )
 *
 *    display( x, y, w, h )      - begins capture /dev/fb0 
 *    capture( w, h )            - begins capture to internal buffer
 *
 *    grab()                     - returns an image
 *    stop()                     - stop capture
 *
 *    setContrast( 0-100|"auto" )
 *    setBrightness( 0-100|"auto" )
 *
 * Properties:
 *    name:           Name of camera (driver actually)
 *    type:           513 (type of USB device?)
 *    minWidth:       in pixels
 *    maxWidth:       in pixels
 *    minHeight:      in pixels
 *    maxHeight:      in pixels
 *    displayX:       leftmost pixel of display
 *    displayY:       top pixel of display
 *    width:          width of current capture
 *    height:         height of current capture buffer
 *    depth:          color depth
 *    color:          boolean - true for color or false for B/W
 *
 * Change History : 
 *
 * $Log: jsCamera.h,v $
 * Revision 1.1  2003-04-21 00:25:19  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSCamera( JSContext *cx, JSObject *glob );
void shutdownCamera();

#endif

