#ifndef __PING_H__
#define __PING_H__ "$Id: ping.h,v 1.1 2003-08-23 02:50:26 ericn Exp $"

/*
 * ping.h
 *
 * This header file declares the pinger_t class,
 * which is a thread (started in its' constructor)
 * class which pings the specified IP address(es)
 * and delivers events to its' virtual event() method
 * on receipt of responses.
 *
 * Change History : 
 *
 * $Log: ping.h,v $
 * Revision 1.1  2003-08-23 02:50:26  ericn
 * -added Javascript ping support
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include <pthread.h>

class pinger_t {
public:
   pinger_t( char const *hostName );
   virtual ~pinger_t( void ); // thread is shut down in destructor

   bool initialized( void ){ return 0 <= fd_ ; }
   char const *errorMsg( void ) const { return ( 0 == errorMsg_ ) ? "uninitialized" : errorMsg_ ; }

   virtual void response( unsigned long ipAddr );

   friend void *pinger_thread( void *arg );
private:
   char const   *hostName_ ;
   unsigned long targetAddr_ ;
   char const   *errorMsg_ ;
   int           fd_ ;
   bool volatile cancel_ ;
   pthread_t     thread_ ;
};

#endif

