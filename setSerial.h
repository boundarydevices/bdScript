#ifndef __SETSERIAL_H__
#define __SETSERIAL_H__ "$Id: setSerial.h,v 1.2 2006-11-22 17:17:24 ericn Exp $"

/*
 * setSerial.h
 *
 * This header file declares some utility routines
 * for setting serial port parameters.
 *
 *
 * Change History : 
 *
 * $Log: setSerial.h,v $
 * Revision 1.2  2006-11-22 17:17:24  ericn
 * -add setRTS() routine
 *
 * Revision 1.1  2006/09/27 01:41:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

// returns zero for success, non-zero for errors

int setBaud( int fd, unsigned baud );
int setParity( int fd, char parity ); // N,E,O
int setDataBits( int fd, unsigned bits ); // 7,8
int setStopBits( int fd, unsigned bits ); // 1,2
int setRaw( int fd ); // no echo or character processing
int setRTS( int fd, bool asserted );

#endif

