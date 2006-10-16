#ifndef __MULTISIGNAL_H__
#define __MULTISIGNAL_H__ "$Id: multiSignal.h,v 1.3 2006-10-16 22:27:30 ericn Exp $"

/*
 * multiSignal.h
 *
 * This header file declares an interface for multiplexing
 * or broadcasting signals to multiple handlers. This is useful
 * for things like the vertical sync handler that may be useful
 * in more than one context.
 *
 * The rtSignal module defines an array of handlers and their
 * context's. A single root signal handler routine is used to 
 * dispatch the signal to all installed handler instances.
 *
 * Note that as of this writing, a maximum of 16 signals may be 
 * used with the multiplexed interface, and a maximum of 16 handlers
 * may be installed for each.
 *
 *
 * Change History : 
 *
 * $Log: multiSignal.h,v $
 * Revision 1.3  2006-10-16 22:27:30  ericn
 * -removed setSignalMask() routine, added dumpSignalHandlers()
 *
 * Revision 1.2  2006/10/10 20:42:53  ericn
 * -add setSignalMask() routine
 *
 * Revision 1.1  2006/09/23 15:28:12  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include <signal.h>

typedef void (*signalHandler_t)( int signo, void *param);

void setSignalHandler
   ( int              signo, 
     sigset_t        &mask,    // mask these signals. Reset on each call
     signalHandler_t  handler,
     void            *handlerContext );

void clearSignalHandler
   ( int             signo,
     signalHandler_t handler,
     void           *handlerContext );

void dumpSignalHandlers( int signo );

#endif

