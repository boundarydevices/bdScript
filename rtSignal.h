#ifndef __RTSIGNAL_H__
#define __RTSIGNAL_H__ "$Id: rtSignal.h,v 1.3 2006-09-23 15:15:27 ericn Exp $"

/*
 * rtSignal.h
 *
 * This header file declares the nextRtSignal() routine
 * for allocating signal numbers to an application.
 * 
 * The first signal allocated will be SIGRTMIN, followed
 * by SIGRTMIN+1...
 *
 * Note that real-time signals with lower numeric value have 
 * higher priority if that matters to you.
 *
 * Change History : 
 *
 * $Log: rtSignal.h,v $
 * Revision 1.3  2006-09-23 15:15:27  ericn
 * -added comment
 *
 * Revision 1.2  2006/09/10 01:14:36  ericn
 * -return active signal range
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

extern int nextRtSignal( void );

extern int minRtSignal( void );
extern int maxRtSignal( void );

#endif

