#ifndef __JSBCWIDTHS_H__
#define __JSBCWIDTHS_H__ "$Id: jsBCWidths.h,v 1.1 2004-05-07 13:32:58 ericn Exp $"

/*
 * jsBCWidths.h
 *
 * This header file declares the initialization routine for the
 * barcodeWidths() Javascript routine, which will return a string 
 * representing the widths of barcode bars and spaces (only sufficient 
 * for linear codes).
 *
 * The string is used as a binary encoding of a variable number of
 * elements, each in the range [1..255] representing the width of 
 * each bar (even-numbered elements) and space (odd-numbered elements).
 * (zero-width would be pointless, eh?)
 *
 * The result always starts and ends with a bar, so the return value
 * should always have an odd length (there's an implicit space at each
 * end).
 *
 * For example, if there were a code which could return a two-bar
 * barcode, the following return value would represent a three-pixel
 * bar, followed by a six-pixel space, then a six-pixel bar:
 *
 *       "\x03\x06\x06"
 *
 * The input parameters to the barcodeWidths() routine are
 *
 * object barcodeWidths( int    symbology,         // enumeration from barcodeReader
 *                       string encodeThis,        // data to be encoded
 *                       int    desiredWidth );    // max width on display or printer
 *
 * The routine will return an object with two members:
 *
 * {
 *    widths:  string ;    // as declared above
 *    length:  int ;       // sum of widths
 * }
 *
 * or false if something fails (unknown code, bad input data)
 *
 * Change History : 
 *
 * $Log: jsBCWidths.h,v $
 * Revision 1.1  2004-05-07 13:32:58  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "js/jsapi.h"

bool initJSBCWidths( JSContext *cx, JSObject *glob );


#endif

