#ifndef __JSBARCODE_H__
#define __JSBARCODE_H__ "$Id: jsBarcode.h,v 1.1 2002-11-17 00:51:34 ericn Exp $"

/*
 * jsBarcode.h
 *
 * This header file declares the barcode initialization
 * routines for Javascript applications. The Javascript
 * interface includes a single event handler:
 *
 *    onBarcode( "code goes here" );
 *
 * and a couple of utility routines:
 *
 *    getBarcode()            - returns barcode digit string of
 *                              last barcode
 *    getBarcodeSymbology()   - returns barcode symbology of
 *                              last barcode
 *
 *
 * It also declares the stopBarcodeThread() routine, to shut
 * down the barcode reader thread on program shutdown.
 * 
 * Change History : 
 *
 * $Log: jsBarcode.h,v $
 * Revision 1.1  2002-11-17 00:51:34  ericn
 * -Added Javascript barcode support
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSBarcode( JSContext *cx, JSObject *glob );

void stopBarcodeThread();

#endif

