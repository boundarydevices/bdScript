#ifndef __JSUSBLP_H__
#define __JSUSBLP_H__ "$Id: jsUsblp.h,v 1.1 2006-10-29 21:59:10 ericn Exp $"

/*
 * jsUsblp.h
 *
 * This header file declares the initialization routine
 * for the usblp() Javascript class, which provides for
 * bi-directional communication with a usblp printer.
 *
 * Construction of a usblp device (channel) is simple:
 *
 *    var mylp = new usblp(); // or usblp( "/dev/usb/lp1" )
 *
 * From there, there are two primary interfaces, read() and
 * write(string):
 *
 *    mylp.write( "send me to printer\n" );
 *
 *    print( "result == ", mylp.read(), "\n" );
 *
 * Data written to and read from the printer is buffered (16k/4k).
 *
 * In addition to the read() and write() calls, an event handler
 * may be associated through the onDataIn() method. If defined, 
 * this method will be invoked each time data is received by the
 * printer, after data has been queued. Typical use is to read
 * and process the input data as shown in the following example:
 *
 *    mylp.onDataIn = function()
 *    {
 *       var in ;
 *       while( ( false !== ( in = this.read() ) ) )
 *          print( "rx: ", escape(in), "\n" );
 *    }
 *
 * Finally, for debugging purposes, the output to the usblp
 * device may be captured to a file for later examination.
 * This is particularly useful when the output is PostScript
 * and may be viewed or translated elsewhere (e.g. demos).
 * 
 * Usage is:
 *
 *    lp.startLog( "/tmp/printData.ps" );
 *
 *    // send data to the printer
 *    
 *    lp.stopLog();
 *
 * Change History : 
 *
 * $Log: jsUsblp.h,v $
 * Revision 1.1  2006-10-29 21:59:10  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "js/jsapi.h"

bool initJSUsbLp( JSContext *cx, JSObject *glob );

#endif

