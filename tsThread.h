#ifndef __TSTHREAD_H__
#define __TSTHREAD_H__ "$Id: tsThread.h,v 1.2 2002-11-08 13:56:59 ericn Exp $"

/*
 * tsThread.h
 *
 * This header file declares a touch-screen reader thread
 * object. It is implemented as an object to simplify shutdown.
 *
 *
 * Change History : 
 *
 * $Log: tsThread.h,v $
 * Revision 1.2  2002-11-08 13:56:59  ericn
 * -modified to use tslib
 *
 * Revision 1.1  2002/11/03 15:39:36  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include <sys/time.h>
#include <tslib.h>

class touchScreenThread_t {
public:
   touchScreenThread_t( void );
   virtual ~touchScreenThread_t( void );

   virtual void onTouch( unsigned        x, 
                         unsigned        y );
   virtual void onRelease( void );
   virtual void onMove( unsigned        x, 
                        unsigned        y );

   bool isOpen( void ) const { return 0 != tsDevice_ ; }
   
   bool begin( void );

   bool isRunning( void ) const { return 0 <= threadHandle_ ; }

   int            threadHandle_ ;
   tsdev *        tsDevice_ ;
//   int fdDevice_ ;
};

#endif

