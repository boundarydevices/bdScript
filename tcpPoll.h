#ifndef __TCPPOLL_H__
#define __TCPPOLL_H__ "$Id: tcpPoll.h,v 1.1 2004-02-08 10:34:22 ericn Exp $"

/*
 * tcpPoll.h
 *
 * This header file declares the tcpPollHandler_t classes, which
 * is used to make a connection to a TCP server for HTTP, etc.
 *
 * Change History : 
 *
 * $Log: tcpPoll.h,v $
 * Revision 1.1  2004-02-08 10:34:22  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#ifndef __POLLHANDLER_H__
#include "pollHandler.h"
#endif 

class tcpHandler_t ;


class tcpClient_t : public pollClient_t {
public:
   virtual ~tcpClient_t( void );
   virtual void onConnect( tcpHandler_t & );
   virtual void onDisconnect( tcpHandler_t & );
};


class tcpHandler_t : public pollHandler_t {
public:
   tcpHandler_t( unsigned long     serverIP,          // network byte order
                 unsigned short    serverPort,        // network byte order
                 pollHandlerSet_t &set,
                 tcpClient_t      *client = 0 );
   virtual ~tcpHandler_t( void );

   enum state_e {
      connecting,
      connected,
      closed
   };

   state_e getState( void ) const { return state_ ; }
   void addClient( tcpClient_t &client );
   void removeClient( tcpClient_t &client );

   //
   // ---------------------------------------------------------------
   // poll-handler callbacks (deferred to tcpHandler_t by default)
   // ---------------------------------------------------------------
   //
   virtual void onDataAvail( void );     // POLLIN
   virtual void onWriteSpace( void );    // POLLOUT
   virtual void onError( void );         // POLLERR
   virtual void onHUP( void );           // POLLHUP

   void checkStatus( void );

protected:
   struct client_t {
      tcpClient_t *client_ ;
      client_t    *next_ ;
   };
   
   unsigned long const  serverIP_ ;
   unsigned short const serverPort_ ;
   state_e              state_ ;
   client_t            *clients_ ;
};


#endif

