#ifndef __BAUDRATE_H__
#define __BAUDRATE_H__ "$Id: baudRate.h,v 1.1 2004-03-27 20:24:22 ericn Exp $"

/*
 * baudRate.h
 *
 * This header file declares the baudRateToConst() and constToBaudRate()
 * routines, which will translate baud rate between bps numbers and their
 * corresponding termios constants.
 *
 *
 * Change History : 
 *
 * $Log: baudRate.h,v $
 * Revision 1.1  2004-03-27 20:24:22  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


bool baudRateToConst( unsigned bps, unsigned &constant );

bool constToBaudRate( unsigned constant, unsigned &bps );


#endif

