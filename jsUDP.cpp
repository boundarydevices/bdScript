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
 * Revision 1.2  2003-11-04 00:41:24  tkisky
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
#include "jsGlobals.h"

struct udpSocket_t {
   int            fd_ ;
   unsigned short port_ ;
   jsval          object_ ;
   jsval          handler_ ;
   jsval          msg_ ;
   jsval          senderIp_ ;
   jsval          senderPort_ ;
   JSObject      *handlerObj_ ;
   JSContext     *cx_ ;
   pthread_t      tHandle_ ;

   void cleanup( JSContext *cx );
};


void udpSocket_t::cleanup( JSContext *cx )
{
   if( 0 <= fd_ )
      close( fd_ );

   pthread_cancel( tHandle_ );
   void *exitStat ;
   pthread_join( tHandle_, &exitStat );

   JS_RemoveRoot( cx, &object_ );
   JS_RemoveRoot( cx, &handler_ );
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
            int const numSent = sendto( udpSocket->fd_, data, dataLen, 0, (struct sockaddr *)&remote, sizeof( remote ) );
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
      udpSocket->cleanup( cx );
      delete udpSocket ;
   }
}

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

static JSBool jsUDP( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsUDPClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root

         udpSocket_t *const sockInfo = new udpSocket_t ;

         sockInfo->fd_ = socket( AF_INET, SOCK_DGRAM, 0 );
         if( 0 <= sockInfo->fd_ )
         {
            JS_SetPrivate( cx, thisObj, sockInfo ); // root?

            sockInfo->port_       = 0 ;
            sockInfo->object_     = JSVAL_VOID ;
            sockInfo->handlerObj_ = obj ;
            sockInfo->handler_    = JSVAL_VOID ;
            sockInfo->msg_        = JSVAL_VOID ;
            sockInfo->senderIp_   = JSVAL_VOID ;
            sockInfo->senderPort_ = JSVAL_VOID ;
            sockInfo->cx_         = cx ;

            JSObject *rhObj = JSVAL_TO_OBJECT( argv[0] );
            if( rhObj )
            {
               jsval jsv ;
               if( JS_GetProperty( cx, rhObj, "port", &jsv )
                   &&
                   JSVAL_IS_INT( jsv ) )
               {
                  sockInfo->port_ = JSVAL_TO_INT( jsv );
                  sockaddr_in myAddress ;

                  memset( &myAddress, 0, sizeof( myAddress ) );

                  myAddress.sin_family      = AF_INET;
                  myAddress.sin_addr.s_addr = 0 ; // local
                  myAddress.sin_port        = htons( sockInfo->port_ );
            
                  if( 0 == bind( sockInfo->fd_, (struct sockaddr *) &myAddress, sizeof( myAddress ) ) )
                  {
                  }
                  else
                  {
                     JS_ReportError( cx, "%m binding to port %u\n", sockInfo->port_ );
                  }
               }
               
               if( JS_GetProperty( cx, rhObj, "onMsg", &jsv )
                   &&
                   ( JSTYPE_FUNCTION == JS_TypeOfValue( cx, jsv ) ) )
               {
                  sockInfo->handler_ = jsv ;
                  if( JS_GetProperty( cx, rhObj, "onMsgObj", &jsv )
                      &&
                      JSVAL_IS_OBJECT( jsv ) )
                  {
                     sockInfo->object_        = jsv ;
                     sockInfo->handlerObj_ = JSVAL_TO_OBJECT( jsv );
                  }
               }
            }

            JS_AddRoot( cx, &sockInfo->object_ );
            JS_AddRoot( cx, &sockInfo->handler_ );
            JS_AddRoot( cx, &sockInfo->msg_ );
            JS_AddRoot( cx, &sockInfo->senderIp_ );
            JS_AddRoot( cx, &sockInfo->senderPort_ );

            int const create = pthread_create( &sockInfo->tHandle_, 0, recvThread, sockInfo );
            if( 0 == create )
            {
            }
            else
               JS_ReportError( cx, "%m:displayThread" );
         }
         else
            JS_ReportError( cx, "creating socket %m" );
      }
      else
         JS_ReportError( cx, "Error allocating udpSocket" );
   }
   else
      JS_ReportError( cx, "Usage: new udpSocket( {[port:NNNN] [,onMsg:handler[,onMsgObj:object]]} );\n" );
   
   return JS_TRUE ;
}

bool initJSUDP( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsUDPClass_,
                                  jsUDP, 1,
                                  udpSocketProperties_, 
                                  udpSocketMethods_, 
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      JS_ReportError( cx, "initializing udpSocket class\n" );
   
   return false ;
}
