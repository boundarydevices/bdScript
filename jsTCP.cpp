/*
 * Module jsTCP.cpp
 *
 * This module defines the Javascript wrappers
 * for TCP sockets as declared in jsTCP.h
 *
 *
 * Change History : 
 *
 * $Log: jsTCP.cpp,v $
 * Revision 1.8  2004-02-07 18:42:50  ericn
 * -simplified, simplified, simplified
 *
 * Revision 1.7  2004/01/01 20:10:44  ericn
 * -got rid of secondary thread
 *
 * Revision 1.6  2003/07/03 13:36:38  ericn
 * -misc multi-tasking and multi-threading fixes
 *
 * Revision 1.5  2003/01/05 01:58:15  ericn
 * -added identification of threads
 *
 * Revision 1.4  2002/12/17 15:03:37  ericn
 * -removed debug stuff
 *
 * Revision 1.3  2002/12/16 14:25:41  ericn
 * -removed warning messages
 *
 * Revision 1.2  2002/12/16 14:20:14  ericn
 * -added mutex, fixed AddRoot, removed debug msgs
 *
 * Revision 1.1  2002/12/15 20:02:58  ericn
 * -added module jsTCP
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "jsTCP.h"
#include "js/jscntxt.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include "codeQueue.h"
#include "ddtoul.h"
#include "stringList.h"
#include "semClasses.h"
#include "jsGlobals.h"
#include <sys/ioctl.h>

class tcpHandler_t : public pollHandler_t {
public:
   tcpHandler_t( JSContext     *cx,
                 JSObject      *scope,
                 unsigned long  serverIP,       // network order
                 unsigned short serverPort );   //    "      "
   ~tcpHandler_t( void );

   virtual void onDataAvail( void );     // POLLIN
   virtual void onHUP( void );           // POLLHUP

   bool readln( std::string &s );

private:
   stringList_t  lines_ ;    // list of input strings
   std::string   prevTail_ ; // previously un-terminated line
   JSContext    *cx_ ;
   JSObject     *scope_ ;
};

tcpHandler_t::tcpHandler_t
   ( JSContext     *cx,
     JSObject      *scope,
     unsigned long  serverIP,       // network order
     unsigned short serverPort )    //    "      "
   : pollHandler_t( socket( AF_INET, SOCK_STREAM, 0 ), pollHandlers_ )
   , cx_( cx )
   , scope_( scope )
{
   if( 0 <= getFd() )
   {
      fcntl( getFd(), F_SETFD, FD_CLOEXEC );
      sockaddr_in serverAddr ;
   
      memset( &serverAddr, 0, sizeof( serverAddr ) );
   
      serverAddr.sin_family      = AF_INET;
      serverAddr.sin_addr.s_addr = serverIP ;
      serverAddr.sin_port        = serverPort ;
   
      if( 0 == connect( getFd(), (struct sockaddr *) &serverAddr, sizeof( serverAddr ) ) )
      {
         int doit = 1 ;
         setsockopt( getFd(), IPPROTO_TCP, TCP_NODELAY, &doit, sizeof( doit ) );
   
         jsval jsv = JSVAL_TRUE ;
         JS_DefineProperty( cx, scope, "isConnected", jsv, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
         setMask( POLLIN|POLLHUP );
         pollHandlers_.add( *this );
      }
      else
      {
         JS_ReportError( cx, "tcpClient:connect" );
         close();
      }
   }
   else
      JS_ReportError( cx, "tcpClient:socket create" );
}

bool tcpHandler_t::readln( std::string &s )
{
   if( !lines_.empty() )
   {
      s = lines_.front();
      lines_.pop_front();
      return true ;
   }
   else
      return false ;
}

void tcpHandler_t::onDataAvail( void )      // POLLIN
{
   int numAvail ;
   int const ioctlResult = ioctl( getFd(), FIONREAD, &numAvail );
   if( ( 0 == ioctlResult )
       &&
       ( 0 < numAvail ) )
   {
      char inData[2048];
      int numRead ;
      
      std::string tail = prevTail_ ;
      prevTail_ = "" ;
      int readSize ;

      while( ( 0 < numAvail )
             &&
             ( 0 < ( readSize = ( ( numAvail > sizeof( inData ) ) 
                                  ? sizeof( inData )
                                  : numAvail ) ) )
             &&
             ( 0 <= ( numRead = read( getFd(), inData, readSize ) ) ) )
      {
         numAvail -= numRead ;
         
         for( int i = 0 ; i < numRead ; i++ )
         {
            char const inChar = inData[i];
            if( ( '\r' == inChar ) || ( '\n' == inChar ) )
            {
               if( 0 < tail.size() )
                  lines_.push_back( tail ); // flush line
               tail = "" ;
            }
            else
               tail += inChar ;
         } // 

         readSize = numAvail ;
         if( readSize > sizeof( inData ) )
            readSize = sizeof( inData );
      }

      prevTail_ = tail ;

      if( 0 < lines_.size() )
      {
         jsval jsv ;

         if( JS_GetProperty( cx_, scope_, "onLineIn", &jsv ) )
         {
            JSType const jst = JS_TypeOfValue( cx_, jsv );
            if( ( JSTYPE_STRING == jst ) ||  ( JSTYPE_FUNCTION == jst ) )
            {
               executeCode( scope_, jsv, "tcpClient.onLineIn" );
            }
            else
               JS_ReportError( cx_, "invalid type %u for tcpClient.onLineIn\n" );
         }
      } // fire JS handler
   }
   else if( 0 == ioctlResult )
   {
      printf( "?? closed ??\n" );
      onHUP();
   }
   else
      perror( "FIONREAD" );
}

void tcpHandler_t::onHUP( void )      // POLLHUP
{
   if( 0 < prevTail_.size() )
   {
      lines_.push_back( prevTail_ );
      prevTail_ = "" ;
      jsval jsv ;

      if( JS_GetProperty( cx_, scope_, "onLineIn", &jsv ) )
      {
         JSType const jst = JS_TypeOfValue( cx_, jsv );
         if( ( JSTYPE_STRING == jst ) ||  ( JSTYPE_FUNCTION == jst ) )
         {
            executeCode( scope_, jsv, "tcpClient.onLineIn" );
         }
         else
            JS_ReportError( cx_, "invalid type %u for tcpClient.onLineIn\n" );
      }
   } // flush tail-end data

   close();
   
   jsval jsv = JSVAL_FALSE ;
   JS_SetProperty( cx_, scope_, "isConnected", &jsv );
   

   if( JS_GetProperty( cx_, scope_, "onClose", &jsv ) )
   {
      JSType const jst = JS_TypeOfValue( cx_, jsv );
      if( ( JSTYPE_STRING == jst ) ||  ( JSTYPE_FUNCTION == jst ) )
      {
         executeCode( scope_, jsv, "tcpClient.onClose" );
      }
      else
         JS_ReportError( cx_, "invalid type %u for tcpClient.onClose\n" );
   }

   setMask( 0 ); // no more events
}

tcpHandler_t::~tcpHandler_t( void )
{
}

static void closeSocket( tcpHandler_t &socketData,
                         JSContext    *cx, 
                         JSObject     *obj )
{
   socketData.onHUP();
}

JSBool
jsTCPClientSend( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      tcpHandler_t *const socketData = (tcpHandler_t *)JS_GetPrivate( cx, obj );
      if( socketData )
      {
         if( 0 <= socketData->getFd() )
         {
            JSString *sArg = JSVAL_TO_STRING( argv[0] );
            unsigned const len = JS_GetStringLength( sArg );
            int numSent = send( socketData->getFd(), JS_GetStringBytes( sArg ), len, 0 );
            if( len == (unsigned)numSent )
            {
               *rval = JSVAL_TRUE ;
            }
            else
            {
               JS_ReportError( cx, "tcpClientSend error %m" );
               closeSocket( *socketData, cx, obj );
            }
         }
         else
            JS_ReportError( cx, "send on closed socket" );
      }
      else
         JS_ReportError( cx, "no socket data" );
   } // need a single string parameter
   else
      JS_ReportError( cx, "Usage: tcpClient().send( string )" );

   return JS_TRUE ;
}

JSBool
jsTCPClientReadln( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( 0 == argc )
   {
      tcpHandler_t *const socketData = (tcpHandler_t *)JS_GetPrivate( cx, obj );
      if( socketData )
      {
         std::string s ;
         if( socketData->readln( s ) )
         {
fprintf( stderr, "---> host data %s\n", s.c_str() );
            *rval = STRING_TO_JSVAL( JS_NewStringCopyN( cx, s.c_str(), s.size() ) );
         }
      }
      else
         JS_ReportError( cx, "no socket data" );
   } // no parameters
   else
      JS_ReportError( cx, "Usage: tcpClient().readln()" );

   return JS_TRUE ;
}

JSBool
jsTCPClientClose( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( 0 == argc )
   {
      tcpHandler_t *const socketData = (tcpHandler_t *)JS_GetPrivate( cx, obj );
      if( socketData )
      {
         if( 0 <= socketData->getFd() )
         {
            closeSocket( *socketData, cx, obj );
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "tcpClientClose on closed socket" );
      }
      else
         JS_ReportError( cx, "no socket data" );
   } // no parameters accepted
   else
      JS_ReportError( cx, "Usage: tcpClient().close()" );

   return JS_TRUE ;
}

void jsTCPClientFinalize(JSContext *cx, JSObject *obj)
{
   if( obj )
   {
      tcpHandler_t *const socketData = (tcpHandler_t *)JS_GetPrivate( cx, obj );
      if( socketData )
      {
         JS_SetPrivate( cx, obj, 0 );
         if( 0 <= socketData->getFd() )
            socketData->close();       // no completion handler
         delete socketData ;
      } // have socket data
//      else
//         printf( "no socket data\n" );
// this seems to be normal, and unrelated to finalization of a TCP client object
//
   }
   else
      printf( "TCP finalize NULL object\n" );
}

static JSFunctionSpec tcpClientMethods_[] = {
    {"send",         jsTCPClientSend,           1 },
    {"readln",       jsTCPClientReadln,         0 },
    {"close",        jsTCPClientClose,          0 },
    {0}
};

enum jsTCPClient_tinyId {
   TCPCLIENT_ISCONNECTED,
   TCPCLIENT_ONLINEIN,
   TCPCLIENT_ONCLOSE,
};

JSClass jsTCPClientClass_ = {
  "tcpClient",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsTCPClientFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec tcpClientProperties_[] = {
  {"isConnected",   TCPCLIENT_ISCONNECTED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"onLineIn",      TCPCLIENT_ONLINEIN,      JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"onClose",       TCPCLIENT_ONCLOSE,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static char const tcpClientUsage[] = {
   "Usage : new tcpClient( { serverPort:int\n"
   "                         [,serverIP:192.168.0.1]\n"
   "                         [,onLineIn:jsCode]\n"
   "                         [,onClose:jsCode] } )\n"
};

static JSBool tcpClient( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) 
       && 
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *const rhObj = JSVAL_TO_OBJECT( argv[0] );
      jsval vServerPort ;
      if( JS_GetProperty( cx, rhObj, "serverPort", &vServerPort ) 
          &&
          JSVAL_IS_INT( vServerPort )  )
      {
         std::string serverIP = "127.0.0.1" ;
         jsval vServerIP ;
         if( JS_GetProperty( cx, rhObj, "serverIP", &vServerIP ) 
             &&
             JSVAL_IS_STRING( vServerIP ) )
         {
            JSString *sIP = JSVAL_TO_STRING( vServerIP );
            serverIP.assign( JS_GetStringBytes( sIP ), JS_GetStringLength( sIP ) );
         }
         
         ddtoul_t fromDotted( serverIP.c_str() );
         if( fromDotted.worked() )
         {
            JSObject *const thisObj = JS_NewObject( cx, &jsTCPClientClass_, NULL, NULL );
      
            if( thisObj )
            {
               *rval = OBJECT_TO_JSVAL( thisObj ); // root
               jsval jsv = JSVAL_FALSE ;
               JS_DefineProperty( cx, thisObj, "isConnected", jsv, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
   
               if( JS_GetProperty( cx, rhObj, "onLineIn", &jsv ) )
               {
                  JSType const jst = JS_TypeOfValue( cx, jsv );
                  if( ( JSTYPE_STRING == jst ) ||  ( JSTYPE_FUNCTION == jst ) )
                  {
                     JS_DefineProperty( cx, thisObj, "onLineIn", jsv, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  }
                  else
                     JS_ReportError( cx, "onLineIn member must be string or function" );
               }
   
               if( JS_GetProperty( cx, rhObj, "onClose", &jsv ) 
                   &&
                   JSVAL_IS_STRING( jsv ) )
                  JS_DefineProperty( cx, thisObj, "onClose", jsv, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
   
               tcpHandler_t *const socketData = new tcpHandler_t( cx, thisObj, 
                                                                  fromDotted.networkOrder(),
                                                                  htons( (unsigned short)JSVAL_TO_INT(vServerPort) ) );
               JS_SetPrivate( cx, thisObj, socketData );
            }
            else
               JS_ReportError( cx, "allocating TCP client" );
         }
         else
            JS_ReportError( cx, "named hosts not yet implemented" );
      }
      else
         JS_ReportError( cx, tcpClientUsage );
   }
   else
      JS_ReportError( cx, tcpClientUsage ); 
      
   return JS_TRUE ;

}

bool initJSTCP( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsTCPClientClass_,
                                  tcpClient, 1,
                                  tcpClientProperties_, 
                                  tcpClientMethods_,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      return false ;
}

