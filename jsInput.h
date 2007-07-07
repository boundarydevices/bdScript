#ifndef __JSINPUT_H__
#define __JSINPUT_H__ "$Id: jsInput.h,v 1.1 2007-07-07 00:25:52 ericn Exp $"

/*
 * jsInput.h
 *
 * This header file declares the initialization file
 * for some input event Javascript bindings:
 *
 * Completely general-purpose input can be handled
 * like this:
 *
 *	var inObj = onInput( fileName, function( type, code, number ){} )
 *
 * To make sense of these, some constants are defined
 * as well. These map to the constants in linux/input.h:
 *
 * 	EV_SYN
 *	EV_KEY
 *	EV_REL
 *	EV_ABS
 *	EV_MSC
 *	EV_SW
 *
 * ... still to come: 
 *	proper mouse handling
 *	proper keyboard handling
 *
 * Change History : 
 *
 * $Log: jsInput.h,v $
 * Revision 1.1  2007-07-07 00:25:52  ericn
 * -[bdScript] added mouse cursor and input handling
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#include "js/jsapi.h"

bool initJSInput( JSContext *cx, JSObject *glob );

#endif

