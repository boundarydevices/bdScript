/*
 * Module jsPing.cpp
 *
 * This module defines the internals and initialization
 * routine for the pinger class as described in 
 *
 *
 * Change History : 
 *
 * $Log: jsPing.cpp,v $
 * Revision 1.1  2003-08-23 02:50:26  ericn
 * -added Javascript ping support
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsPing.h"
#include "ping.h"
#include "jsGlobals.h"
#include "codeQueue.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

class jsPinger_t : public pinger_t {
public:
   jsPinger_t( char const *hostName,
               JSObject   *jsObj,
               jsval       handler )
      : pinger_t( hostName ), 
        jsObj_( jsObj ),
        jsHandler_( handler )
   {
      JS_AddRoot( execContext_, &jsHandler_ );
   }
   virtual ~jsPinger_t( void ){JS_RemoveRoot( execContext_, &jsHandler_ );}

   virtual void response( unsigned long ipAddr );

   JSObject *jsObj_ ;
   jsval     jsIpAddr_ ; // used to root IP address
   jsval     jsHandler_ ;
};

void jsPinger_t :: response( unsigned long ipAddr )
{
   struct in_addr inaddr ;
   inaddr.s_addr = ipAddr ;
   
   JSString *sIP = JS_NewStringCopyZ( execContext_, inet_ntoa( inaddr ) );
   jsIpAddr_ = STRING_TO_JSVAL( sIP );

   queueUnrootedSource( jsObj_, jsHandler_, "pinger", 1, &jsIpAddr_ );
}

extern JSClass jsPingerClass_ ;

static void jsPingerFinalize(JSContext *cx, JSObject *obj);

JSClass jsPingerClass_ = {
  "pinger",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsPingerFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec pingerProperties_[] = {
  {0,0,0}
};

static JSBool
jsPingCancel( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   pinger_t * const pinger = (pinger_t *)JS_GetInstancePrivate( cx, obj, &jsPingerClass_, NULL );
   if( 0 != pinger )
   {
      printf( "pinger cancelled\n" );
      JS_SetPrivate( cx, obj, 0 );
      delete pinger ;
      *rval = JSVAL_TRUE ;
   }
   return JS_TRUE ;
}

static JSFunctionSpec pingerMethods_[] = {
    {"cancel", jsPingCancel,  0 },
    {0}
};

static void jsPingerFinalize(JSContext *cx, JSObject *obj)
{
   pinger_t * const pinger = (pinger_t *)JS_GetInstancePrivate( cx, obj, &jsPingerClass_, NULL );
   if( 0 != pinger )
   {
      JS_SetPrivate( cx, obj, 0 );
      delete pinger ;
   }
}

static JSBool jsPinger( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 == argc )
       && 
       JSVAL_IS_STRING( argv[0] )
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( cx, argv[1] ) ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsPingerClass_, NULL, NULL );
   
      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root

         JSString *sIP = JSVAL_TO_STRING( argv[0] );
         pinger_t *pinger = new jsPinger_t( JS_GetStringBytes( sIP ), thisObj, argv[1] );
         if( pinger->initialized() )
         {
            JS_SetPrivate( cx, thisObj, pinger );
         }
         else
         {
            JS_ReportError( cx, "%s", pinger->errorMsg() );
            delete pinger ;
         }
      }
      else
         JS_ReportError( cx, "Error allocating pinger" );
   }
   else
      JS_ReportError( cx, "Usage: new pinger( ipaddress, responseHandler );\n" );
   
   return JS_TRUE ;
}

bool initPing( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsPingerClass_,
                                  jsPinger, 1,
                                  pingerProperties_, 
                                  pingerMethods_, 
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      JS_ReportError( cx, "initializing pinger class\n" );
   
   return false ;
}


