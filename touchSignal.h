#ifndef __TOUCHSIGNAL_H__
#define __TOUCHSIGNAL_H__ "$Id: touchSignal.h,v 1.1 2006-08-28 15:45:29 ericn Exp $"

/*
 * touchSignal.h
 *
 * This header file declares the touchSignal_t class, 
 * which is used to establish a signal handler to the
 * touch-screen device and route it to a application-defined
 * handler
 *
 *
 * Change History : 
 *
 * $Log: touchSignal.h,v $
 * Revision 1.1  2006-08-28 15:45:29  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */
#include <sys/time.h>

typedef void (*touchHandler_t)( int x, int y, unsigned pressure, timeval const &tv );

class touchSignal_t {
public:
   static touchSignal_t &get( touchHandler_t handler = 0 );

protected:
   touchSignal_t( void );
   ~touchSignal_t( void );

   int   fd_ ;
};

#endif

