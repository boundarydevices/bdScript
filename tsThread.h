#ifndef __TSTHREAD_H__
#define __TSTHREAD_H__ "$Id: tsThread.h,v 1.5 2003-02-01 18:13:12 ericn Exp $"

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
 * Revision 1.5  2003-02-01 18:13:12  ericn
 * -added touchReset_ flag for testing
 *
 * Revision 1.4  2003/01/06 04:28:30  ericn
 * -made callbacks return bool (false if system shutting down)
 *
 * Revision 1.3  2003/01/04 23:10:41  ericn
 * -added close() method to shut down before deallocation
 *
 * Revision 1.2  2002/11/08 13:56:59  ericn
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

extern bool volatile touchReset_ ;

class touchScreenThread_t {
public:
   touchScreenThread_t( void );
   virtual ~touchScreenThread_t( void );

   //
   // These routines should return false if the program
   // is shutting down
   //
   virtual bool onTouch( unsigned        x, 
                         unsigned        y );
   virtual bool onRelease( void );
   virtual bool onMove( unsigned        x, 
                        unsigned        y );

   bool isOpen( void ) const { return 0 != tsDevice_ ; }
   
   bool begin( void );

   bool isRunning( void ) const { return 0 <= threadHandle_ ; }

   void close( void );

   int            threadHandle_ ;
   tsdev *        tsDevice_ ;
//   int fdDevice_ ;
};

#endif

