#ifndef __JSTTY_H__
#define __JSTTY_H__ "$Id: jsTTY.h,v 1.1 2002-12-27 23:31:12 ericn Exp $"

/*
 * jsTTY.h
 *
 * This header file declares the initialization routine
 * for the javascript TTY interface. Methods of the tty
 * global include:
 *
 * tty.prompt( string )    - sets tty prompt
 * tty.readln()            - returns false or line of input
 * tty.onLineIn( code )    - executes code when a new line input
 *                           of input is available.
 * tty.write( string )     - sends specified output to tty
 * tty.writeln( string )   - same as write, plus linefeed
 *
 *
 * Change History : 
 *
 * $Log: jsTTY.h,v $
 * Revision 1.1  2002-12-27 23:31:12  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "js/jsapi.h"

bool initJSTTY( JSContext *cx, JSObject *glob );

void shutdownTTY( void );

#endif

