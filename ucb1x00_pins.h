#ifndef __UCB1X00_PINS_H__
#define __UCB1X00_PINS_H__ "$Id: ucb1x00_pins.h,v 1.1 2006-09-30 18:32:00 ericn Exp $"

/*
 * ucb1x00_pins.h
 *
 * This header file declares the ucb1x00_get_pin() and
 * ucb1x00_set_pin() routines, for accessing GPIO pins
 * on a UCB-1400 
 *
 *
 * Change History : 
 *
 * $Log: ucb1x00_pins.h,v $
 * Revision 1.1  2006-09-30 18:32:00  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

bool ucb1x00_set_pin( int      fdUCB,     // from touch-screen device, normally /dev/touchscreen/ucb1x00
                      unsigned pinNum,
                      bool     setHigh );
                      
bool ucb1x00_get_pin( int      fdUCB,
                      unsigned pinNum,
                      bool    &value );

//
// Return direction and data for each pin
// if bit is set in outMask, pin is an output
// levels return the state of each pin
//
bool ucb1x00_get_pins( int             fdUCB,
                       unsigned short &outMask,
                       unsigned short &levels );

#endif

