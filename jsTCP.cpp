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
 * Revision 1.5  2003-01-05 01:58:15  ericn
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
#include "codeQueue.h"
#include "ddtoul.h"
#include "stringList.h"
#include "semClasses.h"

static mutex_t mutex_ ;

struct socketData_t {
   int          fd_ ;      // socket file descriptor
   stringList_t lines_ ;   // list of input strings
   JSContext   *cx_ ;
   JSObject    *obj_ ;
};

enum {
   haveCR_ = 1,
   haveLF_ = 2
};

static void *readerThread( void *arg )
{
printf( "socketReader %p (id %x)\n", &arg, pthread_self() );   
   socketData_t *const sd = (socketData_t *)arg ;
   jsval jsv ;

   if( !( JS_GetProperty( sd->cx_, sd->obj_, "onLineIn", &jsv ) && JSVAL_IS_STRING( jsv ) ) )
      jsv = JSVAL_VOID ;

   unsigned char terminators = 0 ;
   std::string   curLine ;
   do {
      char inData[80];
      int numRead = recv( sd->fd_, inData, sizeof( inData ) - 1, 0 );
      if( 0 < numRead )
      {
         for( int i = 0 ; i < numRead ; i++ )
         {
            char const c = inData[i];
            unsigned char mask ;
            if( '\r' == c )
               mask = haveCR_ ;
            else if( '\n' == c )
               mask = haveLF_ ;
            else
               mask = 0 ;

            if( mask )
            {
               if( ( 0 == terminators )
                   ||
                   ( 0 != ( mask & terminators ) ) )
               {
                  terminators = mask ;
                  mutexLock_t lock( mutex_ );
                  sd->lines_.push_back( curLine );
                  curLine = "" ;
                  if( JSVAL_VOID != jsv )
                     queueUnrootedSource( sd->obj_, jsv, "tcpClientRead" );
               } // first or duplicate terminator
               else
               {
                  terminators = 0 ;
               } // have alternate terminator, ignore and reset
            }
            else
            {
               curLine += c ;
               terminators = 0 ;
            }
         }
      }
      else
         break; // eof
   } while( 1 );

   if( 0 != curLine.size() )
   {
      mutexLock_t lock( mutex_ );
      sd->lines_.push_back( curLine );
      curLine = "" ;
      if( JSVAL_VOID != jsv )
         queueUnrootedSource( sd->obj_, jsv, "tcpClientRead" );
   }

   JS_RemoveRoot( sd->cx_, sd->obj_ );

   return 0 ;
}

static void closeSocket( socketData_t &socketData,
                         JSContext    *cx, 
                         JSObject     *obj )
{
   close( socketData.fd_ );
   socketData.fd_ = -1 ;
   jsval jsv = JSVAL_FALSE ;
   JS_SetProperty( cx, obj, "isConnected", &jsv );

   if( JS_GetProperty( cx, obj, "onClose", &jsv ) )
      queueSource( obj, jsv, "tcpClient::onClose" );
}

JSBool
jsTCPClientSend( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      socketData_t *const socketData = (socketData_t *)JS_GetPrivate( cx, obj );
      if( socketData )
      {
         if( 0 <= socketData->fd_ )
         {
            JSString *sArg = JSVAL_TO_STRING( argv[0] );
            unsigned const len = JS_GetStringLength( sArg );
            int numSent = send( socketData->fd_, JS_GetStringBytes( sArg ), len, 0 );
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
      socketData_t *const socketData = (socketData_t *)JS_GetPrivate( cx, obj );
      if( socketData )
      {
         mutexLock_t lock( mutex_ );
         if( !socketData->lines_.empty() )
         {
            std::string const s( socketData->lines_.front() );
            socketData->lines_.pop_front();
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
      socketData_t *const socketData = (socketData_t *)JS_GetPrivate( cx, obj );
      if( socketData )
      {
         if( 0 <= socketData->fd_ )
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
      socketData_t *const socketData = (socketData_t *)JS_GetPrivate( cx, obj );
      if( socketData )
      {
         JS_SetPrivate( cx, obj, 0 );
         if( 0 <= socketData->fd_ )
            close( socketData->fd_ );
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
   "Usage : new tcpClient( { serverSock:int\n"
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
         JSObject *const thisObj = JS_NewObject( cx, &jsTCPClientClass_, NULL, NULL );
   
         if( thisObj )
         {
            *rval = OBJECT_TO_JSVAL( thisObj ); // root
            jsval jsv = JSVAL_FALSE ;
            JS_DefineProperty( cx, thisObj, "isConnected", jsv, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );

            if( JS_GetProperty( cx, rhObj, "onLineIn", &jsv ) 
                &&
                JSVAL_IS_STRING( jsv ) )
               JS_DefineProperty( cx, thisObj, "onLineIn", jsv, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );

            if( JS_GetProperty( cx, rhObj, "onClose", &jsv ) 
                &&
                JSVAL_IS_STRING( jsv ) )
               JS_DefineProperty( cx, thisObj, "onClose", jsv, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
            
            std::string serverIP = "127.0.0.1" ;
            jsval vServerIP ;
            if( JS_GetProperty( cx, rhObj, "serverIP", &vServerIP ) 
                &&
                JSVAL_IS_STRING( vServerIP ) )
            {
               JSString *sIP = JSVAL_TO_STRING( vServerIP );
               serverIP.assign( JS_GetStringBytes( sIP ), JS_GetStringLength( sIP ) );
            }

            socketData_t *const socketData = new socketData_t ;
            socketData->cx_  = cx ;
            socketData->obj_ = thisObj ;
            socketData->fd_  = -1 ;
            JS_SetPrivate( cx, thisObj, socketData );

            ddtoul_t fromDotted( serverIP.c_str() );
            if( fromDotted.worked() )
            {
               int sFd = socket( AF_INET, SOCK_STREAM, 0 );
               if( 0 <= sFd )
               {
                  socketData->fd_ = sFd ;
                  sockaddr_in serverAddr ;

                  memset( &serverAddr, 0, sizeof( serverAddr ) );

                  serverAddr.sin_family      = AF_INET;
                  serverAddr.sin_addr.s_addr = fromDotted.networkOrder(); // server's IP
                  serverAddr.sin_port        = htons( (unsigned short)JSVAL_TO_INT(vServerPort) );

                  if( 0 == connect( sFd, (struct sockaddr *) &serverAddr, sizeof( serverAddr ) ) )
                  {
                     int doit = 1 ;
                     setsockopt( sFd, IPPROTO_TCP, TCP_NODELAY, &doit, sizeof( doit ) );
                     JS_AddRoot( cx, &socketData->obj_ );
                     pthread_t thread ;
                     int create = pthread_create( &thread, 0, readerThread, socketData );

                     jsval jsv = JSVAL_TRUE ;
                     JS_DefineProperty( cx, thisObj, "isConnected", jsv, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  }
                  else
                  {
                     JS_ReportError( cx, "tcpClient:connect" );
                     close( sFd );
                  }
               }
               else
                  JS_ReportError( cx, "tcpClient:socket create" );
            }
            else
               JS_ReportError( cx, "named hosts not yet implemented" );
         }
         else
            JS_ReportError( cx, "allocating TCP client" );
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

