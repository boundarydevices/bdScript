#ifndef __URLPOLL_H__
#define __URLPOLL_H__ "$Id: urlPoll.h,v 1.1 2004-02-01 09:53:18 ericn Exp $"

/*
 * urlPoll.h
 *
 * This header file declares the urlPoll_t class, which
 * is used to post and retrieve data over http.
 *
 *
 * Change History : 
 *
 * $Log: urlPoll.h,v $
 * Revision 1.1  2004-02-01 09:53:18  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#ifndef __POLLHANDLER_H__
#include "pollHandler.h"
#endif 


class urlPoll_t : public pollHandler_t {
public:
   urlPoll_t( unsigned long     serverIP,          // network byte order
              unsigned short    serverPort,        // network byte order
              pollHandlerSet_t &set );
   virtual ~urlPoll_t( void );


   enum state_e {
      closed,
      connecting,
      connected
   };

   state_e getState( void ) const { return state_ ; }

   virtual void onDataAvail( void );     // POLLIN
   virtual void onWriteSpace( void );    // POLLOUT
   virtual void onError( void );         // POLLERR
   virtual void onHUP( void );           // POLLHUP

private:
   void checkStatus( void );
   unsigned long const  serverIP_ ;
   unsigned short const serverPort_ ;
   state_e              state_ ;
};


#endif

