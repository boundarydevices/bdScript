#ifndef __TTYSIGNAL_H__
#define __TTYSIGNAL_H__ "$Id: ttySignal.h,v 1.1 2006-10-16 22:45:51 ericn Exp $"

/*
 * ttySignal.h
 *
 * This header file declares the ttySignal_t class, which
 * sets up stdin as raw and non-blocking and establishes 
 * a signal handler for input.
 *
 * Change History : 
 *
 * $Log: ttySignal.h,v $
 * Revision 1.1  2006-10-16 22:45:51  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include <termios.h>

class ttySignal_t ;

typedef void (*ttySignalHandler_t)( ttySignal_t &dev );

class ttySignal_t {
public:
   ttySignal_t( ttySignalHandler_t handler = 0 );
   ~ttySignal_t( void );

   void setHandler( ttySignalHandler_t );
   inline void clearHandler( void ){ setHandler( 0 ); }

   // handler is called through this routine
   void rxSignal( void );

private:
   struct termios     oldState_ ;
   ttySignalHandler_t handler_ ; 
};

#endif

