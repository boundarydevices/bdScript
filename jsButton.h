#ifndef __JSBUTTON_H__
#define __JSBUTTON_H__ "$Id: jsButton.h,v 1.1 2002-11-21 14:09:52 ericn Exp $"

/*
 * jsButton.h
 *
 * This header file declares the initialization
 * routine for the Javascript button classes and
 * functions.
 *
 * The primary class here is 'button', which is used
 * to represent a touchable item on the screen.
 *
 * Buttons are constructed by passing an anonymous object
 * with the following properties:
 *
 *    x, y         - upper left-hand corner of button
 *    width,height - size of touch area
 *    img          - normal (non-pressed) image background
 *    touchImg     - image background when touched
 *    touchSound   - url of sound to play on touch
 *    releaseSound - url of sound to play on release 
 *    onTouch      - code to execute on a touch within the button's area
 *    onMove       - code to execute on touch movement
 *    onRelease    - code to execute on button release 
 *
 * button( { x:int, y:int,
 *           img:image
 *           [, width:int]
 *           [, height:int]
 *           [, touchImg:image]
 *           [, touchSound:mp3File]
 *           [, releaseSound:mp3File]
 *           [, onTouch:"code"]
 *           [, onMove:"code"]
 *           [, onRelease:"code"]
 *         } );
 *
 * button methods include
 *
 *    isPressed()    - returns true if button is pressed
 *
 * Change History : 
 *
 * $Log: jsButton.h,v $
 * Revision 1.1  2002-11-21 14:09:52  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSButton( JSContext *cx, JSObject *glob );

#endif

