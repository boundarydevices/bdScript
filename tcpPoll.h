#ifndef __TCPPOLL_H__
#define __TCPPOLL_H__ "$Id: tcpPoll.h,v 1.2 2004-12-18 18:29:14 ericn Exp $"

/*
 * tcpPoll.h
 *
 * This header file declares the tcpPollHandler_t classes, which
 * is used to make a connection to a TCP server for HTTP, etc.
 *
 * Note that this class doesn't make too many assumptions about
 * what's done with the data, it simply makes callbacks no
 *
 * Change History : 
 *
 * $Log: tcpPoll.h,v $
 * Revision 1.2  2004-12-18 18:29:14  ericn
 * -added dns support
 *
 * Revision 1.1  2004/02/08 10:34:22  ericn
 * -Initial import
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#ifndef __POLLHANDLER_H__
#include "pollHandler.h"
#endif 

class tcpHandler_t ;

//
// Return value should be true if we should keep polling
// for dataAvail and writeSpaceAvail events, or false to
// ignore the signal (clear the POLL mask)
//
typedef bool (*tcpCallback_t)( tcpHandler_t &tcpHandler,
                               int           event,        // tcpHandler_t::event_e
                               void         *opaque );


class tcpHandler_t : public pollHandler_t {
public:
   tcpHandler_t( char const       *hostName,
                 unsigned short    serverPort,        // network byte order
                 pollHandlerSet_t &set );
   tcpHandler_t( unsigned long     serverIP,          // network byte order
                 unsigned short    serverPort,        // network byte order
                 pollHandlerSet_t &set );
   virtual ~tcpHandler_t( void );             

   enum state_e {
      nameLookup,
      connecting,
      connected,
      closed
   };

   enum event_e {
      nameResolved_e,
      connect_e,
      disconnect_e,
      dataAvail_e,
      writeSpaceAvail_e,
      error_e,
      numEvents
   };

   state_e getState( void ) const { return state_ ; }
   
   void setCallback( event_e       event, 
                     tcpCallback_t callback,
                     void         *opaque );
   tcpCallback_t getCallback( event_e event );
   tcpCallback_t clearCallback( event_e event );

   void onNameLookup( char const   *hostName, 
                      bool          resolved,
                      unsigned long address );
   unsigned long getServerIp() const { return serverIP_ ; }
   unsigned short getServerPort() const { return serverPort_ ; }
   char const *hostName( void ) const { return hostName_ ? hostName_ : "unknown" ; }

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
   char const          *hostName_ ;
   unsigned long        serverIP_ ;
   unsigned short const serverPort_ ;
   state_e              state_ ;
   void                *cbParams_[numEvents];
   tcpCallback_t        callbacks_[numEvents];
};

#endif

