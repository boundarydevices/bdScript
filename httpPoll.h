#ifndef __HTTPPOLL_H__
#define __HTTPPOLL_H__ "$Id: httpPoll.h,v 1.3 2004-12-28 03:44:15 ericn Exp $"

/*
 * httpPoll.h
 *
 * This header file declares the httpPoll_t class, which
 * is used to post and retrieve data over http.
 *
 * Expected usage is something like the following:
 *
 *    pollHandlerSet_t ph ;
 *    httpPoll_t       htp( inet_addr( "192.168.0.1" ), 
 *                          htons( 80 ),
 *                          "/index.html",
 *                          ph );
 *    htp.addHeader( "Referer", "http://www.boundarydevices.com" );
 *    htp.addParam( "myParam", "Value goes here" );
 *    htp.addFile( "myFile", "/tmp/upload.dat" );
 *
 *    if( htp.start() )
 *    {
 *       while( !htp.isComplete() )
 *       {
 *          ph.poll( -1UL );
 *       }
 *    }
 *
 * In general, the URL must be parsed and passed to the constructor,
 * then any custom headers and/or POST parameters are handed to the
 * httpPoll_t object, and the transfer is started through the start()
 * method.
 *
 * All protocol processing will then occur from within the pollHandler_t
 * callbacks, and the application will be notified of completion through
 * the 'isComplete()' method.
 *
 * Change History : 
 *
 * $Log: httpPoll.h,v $
 * Revision 1.3  2004-12-28 03:44:15  ericn
 * -callback-driven
 *
 * Revision 1.2  2004/12/18 18:30:41  ericn
 * -require tcpHandler, not more generic pollHandler, add dns support
 *
 * Revision 1.1  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#ifndef __TCPPOLL_H__
#include "tcpPoll.h"
#endif 

#include <string>
#include <list>

class httpPoll_t ;

//
// Return value should be true if we should keep polling
// for dataAvail and writeSpaceAvail events, or false to
// ignore the signal (clear the POLL mask)
//
typedef bool (*httpCallback_t)( void       *opaque,
                                httpPoll_t &handler,
                                int         event,          // httpPoll_t::event_e
                                void       *eventParam );   // parameter depends on event


class httpPoll_t {
public:
   httpPoll_t();     // from server's root, server is specified through TCP handler
   virtual ~httpPoll_t( void );

   struct header_t {
      std::string fieldName_ ;
      std::string value_ ;
   };

   //
   // Simple application interface
   //
   void addHeader( std::string const &fieldName, std::string const &value );
   void addParam( std::string const &fieldName, std::string const &value );
   void addFile( std::string const &fieldName, std::string const &fileName );

   void setName( char const *fileName ); // server-relative, include leading slash
   void setHandler( tcpHandler_t *h );
   void start( void );
   void clearHandler();

   bool isComplete( void ) const { return ( error == state_ ) || ( complete == state_ ); }

   enum state_e {
      idle,             // waiting for 'start()'
      waitConnect,
      sendHeaders,
      sendBody,
      receiveHeaders,
      receiveChunk,
      receiveBody,
      complete,
      error
   };

   state_e getState( void ) const { return state_ ; }

   // ---------------------------------------------------------------
   //                   HTTP-level callbacks
   // ---------------------------------------------------------------
   enum event_e {
      connect,
      rxData,
      eof,
      numEvents
   };

   void setHandler( event_e         which,
                    httpCallback_t  callback,
                    void           *cbParam );

   // return true if username and password are available
   virtual bool getAuth( char const *&username, char const *&password );

   // called when uploaded data is sent
   virtual void onWriteData( unsigned long writtenSoFar );

   // called to let application know the total write space
   virtual void writeSize( unsigned long numToWrite );

   // called when return size is available (during received header processing)
   virtual void readSize( unsigned long numBytes );

   // called when received bytes is updated 
   // (to make space in the cache for chunked transfers)
   virtual void onReadData( unsigned long readSoFar );

   //
   // ---------------------------------------------------------------
   // poll-handler callbacks (much of the work happens here)
   // ---------------------------------------------------------------
   //
   virtual bool onConnect( tcpHandler_t & );
   virtual bool onDisconnect( tcpHandler_t & );

   virtual bool onDataAvail( pollHandler_t &h );     // POLLIN
   virtual bool onWriteSpace( pollHandler_t &h );    // POLLOUT
   virtual bool onError( pollHandler_t &h );         // POLLERR
   virtual bool onHUP( pollHandler_t &h );           // POLLHUP

protected:
   tcpHandler_t        *h_ ;
   std::string          file_ ;
   state_e              state_ ;
   bool                 started_ ;
   std::list<header_t>  headers_ ;
   std::list<header_t>  params_ ;
   std::list<header_t>  files_ ;
   httpCallback_t       handlers_[numEvents];
   void                *hParams_[numEvents];
};

#endif

