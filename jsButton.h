#ifndef __JSBUTTON_H__
#define __JSBUTTON_H__ "$Id: jsButton.h,v 1.3 2002-12-26 19:26:59 ericn Exp $"

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
 *    onMoveOff    - code to execute when touch moves off of button
 *    onRelease    - code to execute on button release 
 *
 * button( { x:int, y:int,
 *           img:image              // rhs is a fully loaded image object
 *           [, width:int]
 *           [, height:int]
 *           [, touchImg:image]
 *           [, touchSound:mp3File]
 *           [, releaseSound:mp3File]
 *           [, onTouch:"code"]
 *           [, onMove:"code"]
 *           [, onMoveOff:"code"]
 *           [, onRelease:"code"]
 *         } );
 *
 * button methods include
 *
 *    isPressed()    - returns true if button is pressed
 *
 * Or, for those times when you just can't spend the time
 * making images, there's a text-mode version which is faster
 * to create:
 *
 * textButton( { x:int, y:int,
 *               bgColor:0xRRGGBB,
 *               text:"buttonText",
 *               width:int,
 *               height:int,
 *               font:font,                        // fully loaded font object
 *               pointSize:int
 *               [, textColor:0xRRGGBB]            // defaults to black (0x000000)
 *               [, borderWidth:int(#pixels)]      // defaults to 2
 *               [, touchSound:mp3File]
 *               [, releaseSound:mp3File]
 *               [, onTouch:"code"]
 *               [, onMove:"code"]
 *               [, onMoveOff:"code"]
 *               [, onRelease:"code"]
 *             } );
 *
 * This version of button replaces the img and touchImg methods 
 * with a background color, text and text. It also makes size a 
 * required field, and adds optional fields textColor and borderWidth.
 *
 * Change History : 
 *
 * $Log: jsButton.h,v $
 * Revision 1.3  2002-12-26 19:26:59  ericn
 * -added onMoveOff support
 *
 * Revision 1.2  2002/12/07 21:01:40  ericn
 * -added support for text buttons
 *
 * Revision 1.1  2002/11/21 14:09:52  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSButton( JSContext *cx, JSObject *glob );

#endif

