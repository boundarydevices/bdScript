#ifndef __JSSERIAL_H__
#define __JSSERIAL_H__ "$Id: jsSerial.h,v 1.1 2004-03-27 20:24:35 ericn Exp $"

/*
 * jsSerial.h
 *
 * This header file declares the initialization routine for the
 * Javascript serialPort class, which allows access to a standard
 * Linux serial port. Note that there are three major models of reading
 * input from a serial port:
 *
 *    terminated:    useful when each input packet from the device
 *                   is terminated by a single character. This input
 *                   method is also referred to as 'line-oriented'
 *                   since the terminator is often a carriage-return
 *                   or linefeed. The application specifies this mode
 *                   through the use of the 'terminator' parameter
 *                   to the serialPort constructor.
 *
 *                   When this mode is used, the application is notified
 *                   of the receipt of a 'line' through the 'onLineIn()' 
 *                   callback, and the application reads lines through the
 *                   readln() method.
 *
 *                   Note that the readln() routine does not include the
 *                   terminating character.
 *
 *    timed out:     useful when messages are received from an attached
 *                   device, but a terminator cannot be specified. The
 *                   application specifies this mode by:
 *
 *                      a.) Not specifying a terminator to the serialPort
 *                          constructor, and
 *                      b.) Specifying a 'timeout' parameter to the serialPort
 *                          constructor
 *
 *                   In this mode, the serial port will terminate a line
 *                   when at least the specified number of milliseconds
 *                   have elapsed since the receipt of a character.
 *
 *                   In this mode, the serial port will notify the application 
 *                   of input lines through the onLineIn() callback, and the
 *                   application can read the next input line through the 'readln' 
 *                   method.
 *
 *    character:     When neither of the 'terminator' and 'timeout' parameters
 *                   are specified in the serialPort constructor, the serial
 *                   port operates in 'character' mode, and signals the arrival
 *                   of data to the application via the onChar() callback.
 *                   The application reads all available input data through the 
 *                   read() callback.
 *
 * Change History : 
 *
 * $Log: jsSerial.h,v $
 * Revision 1.1  2004-03-27 20:24:35  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "js/jsapi.h"

bool initJSSerial( JSContext *cx, JSObject *glob );



#endif

