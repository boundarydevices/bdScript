#ifndef __JSBARCODE_H__
#define __JSBARCODE_H__ "$Id: jsBarcode.h,v 1.3 2004-05-07 13:32:32 ericn Exp $"

/*
 * jsBarcode.h
 *
 * This header file declares the barcode initialization
 * routines for Javascript applications. The Javascript
 * barcodeReader class has the interface described below.
 *
 * The following are provided by the base barcodeReader 
 * class:
 *
 *    construction:
 *       var myReader = new barcodeReader( device, [read terminator] );
 *
 *    output (for control strings):
 *       myReader.send( "string to send" );
 *
 *    comm setup:
 *       getBaud()         - returns baud rate in bps
 *       setBaud( 9600 )
 *       getParity()       - returns 'E', 'O', 'N'
 *       setParity( 'N' )
 *       getBits()         - returns character length (bits)
 *       setBits( 8 )
 *       getDelay()        - returns inter-character delay (in useconds) for transmit
 *       setDelay( 10000 ) - sets inter-character delay for transmit
 *
 *    properties:
 *       deviceName        - device name string from constructor
 *       terminator        - line terminator from constructor (or NULL)
 *
 *    constants:
 *       unknownSym
 *       upc
 *       i2of5
 *       code39
 *       code128
 *       ean
 *       ean128
 *       codabar
 *       code93
 *       pharmacode
 *
 *       symbologyNames[] - array of symbology names
 *
 * The following routines aren't implemented by the base class, but
 * by scanner-specific methods:
 *
 *    notification: (called by the base class whenever data (or terminated data) comes in)
 *
 *       myReader.onData = function( barcodeString )
 *                            { print( "read from scanner <", barcodeString, ">" );
 *
 *     - the scanner-specific code should call this on a successful decode
 *
 *       myReader.onBarcode = function( barcode, symbology )
 *                            { print( "read from scanner <", barcode, ">, symbology <", barcodeScanner.symbologyNames[symbology] );
 *       myReader.onBadScan = function( message )
 *
 *    enable/disable:
 *       myReader.enable  = function(){ this.send( "enable!" ); }
 *       myReader.disable = function(){ this.send( "disable!" );
 *    
 *    utilities:
 *       initialized()     - called when scanner is done with initializatio
 *       identification()  - call to get identification string
 *
 * It also declares the stopBarcodeThread() routine, to shut
 * down the barcode reader thread on program shutdown.
 * 
 * Change History : 
 *
 * $Log: jsBarcode.h,v $
 * Revision 1.3  2004-05-07 13:32:32  ericn
 * -made internals external (and shareable)
 *
 * Revision 1.2  2003/06/08 15:19:05  ericn
 * -objectified scanner interface
 *
 * Revision 1.1  2002/11/17 00:51:34  ericn
 * -Added Javascript barcode support
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

enum bcrTinyId {
   BCR_NOSYM,      // unknown symbology
   BCR_UPC,        // UPC
   BCR_I2OF5,      // Interleaved 2 of 5
   BCR_CODE39,     // Code 39 (alpha)
   BCR_CODE128,    // Code 128
   BCR_EAN,        // European Article Numbering System
   BCR_EAN128,     //    "        "        "        "
   BCR_CODABAR,    // 
   BCR_CODE93,     // 
   BCR_PHARMACODE, // 
   BCR_NUMSYMBOLOGIES
};

extern char const *const bcrSymNames_[BCR_NUMSYMBOLOGIES];

bool initJSBarcode( JSContext *cx, JSObject *glob );

#endif

