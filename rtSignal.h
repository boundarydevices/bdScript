#ifndef __RTSIGNAL_H__
#define __RTSIGNAL_H__ "$Id: rtSignal.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * rtSignal.h
 *
 * This header file declares the nextRtSignal() routine
 * for allocating signal numbers to an application.
 * 
 * The first signal allocated will be SIGRTMIN, followed
 * by SIGRTMIN+1...
 *
 *
 * Change History : 
 *
 * $Log: rtSignal.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

extern int nextRtSignal( void );

#endif

