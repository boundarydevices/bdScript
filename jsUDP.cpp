/*
 * Module jsUDP.cpp
 *
 * This module defines the Javascript UDP class
 * as described in jsUDP.h
 *
 *
 * Change History : 
 *
 * $Log: jsUDP.cpp,v $
 * Revision 1.3  2003-12-28 20:55:49  ericn
 * -get rid of secondary thread
 *
 * Revision 1.2  2003/11/04 00:41:24  tkisky
 * -htons
 *
 * Revision 1.1  2003/09/10 04:56:30  ericn
 * -Added UDP support
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsUDP.h"
#include "jsGlobals.h"
#include "codeQueue.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "jsGlobals.h"
#include "udpPoll.h"

static JSObject *udpProto = 0 ;

class udpSocket_t : public udpPoll_t {
public:
   udpSocket_t( pollHandlerSet_t &set,
                unsigned short    port, // network byte-order
                JSContext        *cx,
                JSObject         *scope,
                jsval             handler );
   ~udpSocket_t( void );

   virtual void onMsg( void const        *msg,
                       unsigned           msgLen,
                       sockaddr_in const &sender );
protected:
   jsval          object_ ;
   jsval          handler_ ;
   jsval          msg_ ;
   jsval          senderIp_ ;
   jsval          senderPort_ ;
   JSObject      *handlerObj_ ;
   JSContext     *cx_ ;
};

udpSocket_t :: udpSocket_t
   ( pollHandlerSet_t &set,
     unsigned short    port, // network byte-order
     JSContext        *cx,
     JSObject         *scope,
     jsval             handler )
   : udpPoll_t( set, port )
   , object_( OBJECT_TO_JSVAL( scope ) )
   , handler_( handler )
   , msg_( JSVAL_VOID )
   , senderIp_( JSVAL_VOID )
   , senderPort_( JSVAL_VOID )
   , handlerObj_( scope )
   , cx_( cx )
{
   JS_AddRoot( cx, &object_ );
   JS_AddRoot( cx, &handler_ );
   JS_AddRoot( cx, &msg_ );
   JS_AddRoot( cx, &senderIp_ );
   JS_AddRoot( cx, &senderPort_ );
}
   
udpSocket_t :: ~udpSocket_t( void )
{
   JS_RemoveRoot( cx_, &object_ );
   JS_RemoveRoot( cx_, &handler_ );
   JS_RemoveRoot( cx_, &msg_ );
   JS_RemoveRoot( cx_, &senderIp_ );
   JS_RemoveRoot( cx_, &senderPort_ );
}

void udpSocket_t :: onMsg
   ( void const        *msg,
     unsigned           msgLen,
     sockaddr_in const &sender )
{
   msg_ = STRING_TO_JSVAL( JS_NewStringCopyN( cx_, (char const *)msg, msgLen ) );
   senderIp_ = STRING_TO_JSVAL( JS_NewStringCopyZ( cx_, inet_ntoa( sender.sin_addr ) ) );
   senderPort_ = INT_TO_JSVAL( ntohs(sender.sin_port) );
   jsval args[3] = {
      msg_,
      senderIp_,
      senderPort_
   };
   executeCode( handlerObj_, handler_, "udpSocket_t::recvfrom", 3, args );

}

extern JSClass jsUDPClass_ ;

static void jsUDPFinalize(JSContext *cx, JSObject *obj);

JSClass jsUDPClass_ = {
  "udpSocket",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsUDPFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec udpSocketProperties_[] = {
  {0,0,0}
};

static JSBool jsUDPSendTo( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   udpSocket_t * const udpSocket = (udpSocket_t *)JS_GetInstancePrivate( cx, obj, &jsUDPClass_, NULL );
   if( 0 != udpSocket )
   {
      if( ( 3 == argc )
          &&
          JSVAL_IS_STRING( argv[0] )
          &&
          JSVAL_IS_INT( argv[1] )
          &&
          JSVAL_IS_STRING( argv[2] ) )
      {
         int const targetIP = inet_addr( JS_GetStringBytes( JSVAL_TO_STRING( argv[0] ) ) );
         if( INADDR_NONE != targetIP )
         {
            unsigned short const targetPort = JSVAL_TO_INT( argv[1] );
            sockaddr_in remote ;
            remote.sin_family      = AF_INET ;
            remote.sin_addr.s_addr = targetIP ;
            remote.sin_port        = htons(targetPort) ;
            JSString *const sData = JSVAL_TO_STRING( argv[2] );
            char const     *data = JS_GetStringBytes( sData );
            unsigned const  dataLen = JS_GetStringLength( sData );
            int const numSent = sendto( udpSocket->getFd(), data, dataLen, 0, (struct sockaddr *)&remote, sizeof( remote ) );
printf( "sent %d bytes to ip 0x%08lx, port 0x%04x\n", numSent, targetIP, targetPort );
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "Usage: udpSocket.sendto( hostIPString, port, dataString );" );
      }
      else
         JS_ReportError( cx, "Usage: udpSocket.sendto( hostIPString, port, dataString );" );
   }
   else
      JS_ReportError( cx, "sendto:no data" );
   return JS_TRUE ;
}

static JSFunctionSpec udpSocketMethods_[] = {
    {"sendto", jsUDPSendTo,  0 },
    {0}
};

static void jsUDPFinalize(JSContext *cx, JSObject *obj)
{
   udpSocket_t * const udpSocket = (udpSocket_t *)JS_GetInstancePrivate( cx, obj, &jsUDPClass_, NULL );
   if( 0 != udpSocket )
   {
      JS_SetPrivate( cx, obj, 0 );
      delete udpSocket ;
   }
}

/*
#define MAXRX 4096
static void *recvThread( void *arg )
{
   udpSocket_t &udpSocket = *( udpSocket_t * )arg ;
   char *const rxBuf = new char [MAXRX];
   while( 1 )
   {
      struct sockaddr_in from;
      socklen_t fromlen = sizeof(from);
      int const numRx = recvfrom( udpSocket.fd_, rxBuf, MAXRX, 0, (struct sockaddr *) &from, &fromlen );
      if( 0 < numRx )
      {
         if( JSVAL_VOID != udpSocket.handler_ )
         {
            mutexLock_t lock( execMutex_ );
            udpSocket.msg_ = STRING_TO_JSVAL( JS_NewStringCopyN( execContext_, rxBuf, numRx ) );
            udpSocket.senderIp_ = STRING_TO_JSVAL( JS_NewStringCopyZ( execContext_, inet_ntoa( from.sin_addr ) ) );
            udpSocket.senderPort_ = INT_TO_JSVAL( ntohs(from.sin_port) );
            jsval args[3] = {
               udpSocket.msg_,
               udpSocket.senderIp_,
               udpSocket.senderPort_
            };
            queueUnrootedSource( udpSocket.handlerObj_, udpSocket.handler_, "udpSocket_t::recvfrom", 3, args );
         }
      }
      else if( 0 > numRx )
         break;
      else
         printf( "0 UDP bytes\n" );
   }
}
*/

static char const usage[] = {
   "Usage: new udpSocket( {[port:NNNN] [,onMsg:handler[,onMsgObj:object]]} );\n"
};

static JSBool jsUDP( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *rhObj = JSVAL_TO_OBJECT( argv[0] );
      if( rhObj )
      {
         int port = 0 ;
         jsval jsv ;
         if( JS_GetProperty( cx, rhObj, "port", &jsv )
             &&
             JSVAL_IS_INT( jsv ) )
         {
            port = htons( (unsigned short)JSVAL_TO_INT( jsv ) );
         }

         JSObject *scope = obj; // default to global scope
         jsval     handler = JSVAL_VOID ;

         if( JS_GetProperty( cx, rhObj, "onMsg", &handler )
             &&
             ( JSTYPE_FUNCTION == JS_TypeOfValue( cx, handler ) ) )
         {
            jsval jsv ;
            if( JS_GetProperty( cx, rhObj, "onMsgObj", &jsv )
                &&
                JSVAL_IS_OBJECT( jsv ) )
            {
               scope = JSVAL_TO_OBJECT( jsv );
            }
         } 

         
         JSObject *thisObj = JS_NewObject( cx, &jsUDPClass_, udpProto, NULL );
         if( thisObj )
         {
            udpSocket_t *const sockInfo = new udpSocket_t( pollHandlers_, port, cx, scope, handler );
            JS_SetPrivate( cx, thisObj, sockInfo ); // root?
            *rval = OBJECT_TO_JSVAL( thisObj ); // root
         }
         else
            JS_ReportError( cx, "Error allocating udpSocket" );
      }
      else
         JS_ReportError( cx, usage );
   }
   else
      JS_ReportError( cx, usage );
   
   return JS_TRUE ;
}

bool initJSUDP( JSContext *cx, JSObject *glob )
{
   udpProto = JS_InitClass( cx, glob, NULL, &jsUDPClass_,
                            jsUDP, 1,
                            udpSocketProperties_, 
                            udpSocketMethods_, 
                            0, 0 );
   if( udpProto )
   {
      JS_AddRoot( cx, &udpProto );
      return true ;
   }
   else
      JS_ReportError( cx, "initializing udpSocket class\n" );
   
   return false ;
}
