#ifndef __JSSTARUSB_H__
#define __JSSTARUSB_H__ "$Id: jsStarUSB.h,v 1.2 2004-07-04 21:30:08 ericn Exp $"

/*
 * jsStarUSB.h
 *
 * This header file declares the initialization 
 * routines for the starUSB printer class.
 * 
 * The interface to this class attempts to be the
 * same as the printer class:
 *
 *    print( string )      - sends raw data to the printer
 *    flush()              - forces and waits for I/O completion
 *    close()              - closes the printer handle
 *
 * with a couple of extensions for reading status information
 * from the printer. 
 *
 * The following routine returns a status object with a 
 * set of status properties from the printer, or false to
 * indicate that no status is available:
 *
 *    getStatus()
 *
 * Th returned object has the following fields:
 *
 *    paperInPresenter  - bool
 *    counter           - count of receipts removed from printer
 *    paperNearEmpty    - bool
 *    paperOut          - bool
 *    errors            - int (bitmask) - zero if no errors
 *       1  OFFLINE
 *       2  Cover open
 *       4  Overheat
 *       8  Cutter error
 *      16  Paper jam
 *      32  Mechanical error
 *      64  Overflow
 *     128  Command error
 *     256  BM error
 *     512  Head up
 *    1024  Communications error (printer powered off?)
 *
 * In general, it is expected that the software will use the first
 * two members of the status object to control flow of print data
 * to the printer, and the other three to service status to a user
 * or customer.
 *
 * The final routine attaches a call-back method to status transitions
 *
 *    onStatusChange( method [, obj ] )
 *
 * The starUSB class will always call the onStatusChange() routine once
 * to signal the current value.
 *
 * Change History : 
 *
 * $Log: jsStarUSB.h,v $
 * Revision 1.2  2004-07-04 21:30:08  ericn
 * -publish star USB class
 *
 * Revision 1.1  2004/06/28 02:57:34  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include "js/jsapi.h"

extern JSClass jsStarUSBClass_ ;

bool initJSStarUSB( JSContext *cx, JSObject *glob );

#endif

