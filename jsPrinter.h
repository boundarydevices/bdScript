#ifndef __JSPRINTER_H__
#define __JSPRINTER_H__ "$Id: jsPrinter.h,v 1.1 2004-05-05 03:20:32 ericn Exp $"

/*
 * jsPrinter.h
 *
 * This header file declares the initialization routine
 * for the printer class, which is used to connect to a 
 * printer device (usually "/dev/usb/lp0" ).
 *
 * Note that the general interface to the printer class
 * is pretty basic, and that construction of a printer
 * object defers some stuff to the derivative classes:
 *
 *    CBM   - CBM 1220 printers
 *    Star  - Star Micronics TUP-900 printers
 * 
 * When a printer object is 
 *
 * Printer methods include:
 *
 *    print( string )      - sends raw data to the printer
 *    flush()              - forces and waits for I/O completion
 *    close()              - closes the printer handle
 *
 * The printer's constructor requires a device name
 * string (normally "/dev/usb/lp0" ).
 *
 *
 * Change History : 
 *
 * $Log: jsPrinter.h,v $
 * Revision 1.1  2004-05-05 03:20:32  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include "js/jsapi.h"

extern JSObject *printerProto ;
bool initPrinter( JSContext *cx, JSObject *glob );

#endif

