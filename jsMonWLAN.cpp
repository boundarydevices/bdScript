/*
 * Module jsMonWLAN.cpp
 *
 * This module defines the initialization routine for the 
 * WLAN monitoring class as declared and described in jsMonWLAN.h
 *
 *
 * Change History : 
 *
 * $Log: jsMonWLAN.cpp,v $
 * Revision 1.1  2003-08-20 02:54:02  ericn
 * -added module jsMonWLAN
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsMonWLAN.h"
#include "monitorWLAN.h"
#include "jsGlobals.h"
#include "codeQueue.h"
#include "macros.h"

class jsMonitor_t : public monitorWLAN_t {
public:
   jsMonitor_t( JSObject *jsObj )
      : monitorWLAN_t(), jsObj_( jsObj ),
        jsDeviceName_( JSVAL_VOID ),
        jsEventName_( JSVAL_VOID )
   {
      JS_AddRoot( execContext_, &jsDeviceName_ );
      JS_AddRoot( execContext_, &jsEventName_ );
   }
   virtual ~jsMonitor_t( void ){}

   virtual void message( event_t event, char const *deviceName );

   JSObject *jsObj_ ;
   jsval     jsDeviceName_ ;
   jsval     jsEventName_ ;
};

static char const * const eventNames[] = {
   "notconnected",
   "connected",            // note that these echo the constants in 
   "disconnected",         // linux-wlan/src/prism2/include/prism2/hfa384x.h
   "ap_change",
   "ap_outofrange",
   "ap_inrange",
   "assocfail"
};

static unsigned const numEventNames = sizeof( eventNames )/sizeof( eventNames[0] );

void jsMonitor_t :: message( event_t event, char const *deviceName )
{
   if( (unsigned)event < numEventNames )
   {
      char const * const eventName = eventNames[event];
      
      JSString *sDevice = JS_NewStringCopyZ( execContext_, deviceName );
      jsDeviceName_ = STRING_TO_JSVAL( sDevice );
      
      JSString *sEvent = JS_NewStringCopyZ( execContext_, eventName );
      jsEventName_ = STRING_TO_JSVAL( sEvent );
      
      jsval jsv ;
      if( JS_GetProperty( execContext_, jsObj_, eventName, &jsv ) )
      {
         JSType const type = JS_TypeOfValue( execContext_, jsv );
         if( JSTYPE_FUNCTION == type )
         {
            queueUnrootedSource( jsObj_, jsv, "monitorWLAN", 1, &jsDeviceName_ );
            return ;
         }
         else if( JSTYPE_VOID != type )
            JS_ReportError( execContext_, "handler %s is type %d, not a function!", eventName, type );
      }

      if( JS_GetProperty( execContext_, jsObj_, "callback", &jsv ) )
      {
         JSType const type = JS_TypeOfValue( execContext_, jsv );
         if( JSTYPE_FUNCTION == type )
         {
            jsval args[2] = { jsEventName_, jsDeviceName_ };
            queueUnrootedSource( jsObj_, jsv, "monitorWLAN", 2, args );
         }
         else if( JSTYPE_VOID != type )
            JS_ReportError( execContext_, "handler callback is type %d, not a function!", type );
         else
            JS_ReportError( execContext_, "No WLAN monitor callbacks defined" );
      }
      else
         JS_ReportError( execContext_, "No WLAN monitor callbacks defined" );
   }
   else
      JS_ReportError( execContext_, "Invalid wireless event" );
}

extern JSClass jsMonitorWLANClass_ ;

static void jsMonitorWLANFinalize(JSContext *cx, JSObject *obj);

JSClass jsMonitorWLANClass_ = {
  "monitorWLAN",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsMonitorWLANFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec monitorWLANProperties_[] = {
  {0,0,0}
};


static void jsMonitorWLANFinalize(JSContext *cx, JSObject *obj)
{
   monitorWLAN_t * const monitor = (monitorWLAN_t *)JS_GetInstancePrivate( cx, obj, &jsMonitorWLANClass_, NULL );
   if( 0 != monitor )
   {
      JS_SetPrivate( cx, obj, 0 );
      delete monitor ;
   }
}

static JSBool jsMonitorWLAN( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   JSObject *thisObj = JS_NewObject( cx, &jsMonitorWLANClass_, NULL, NULL );

   if( thisObj )
   {
      *rval = OBJECT_TO_JSVAL( thisObj ); // root
      
      monitorWLAN_t *monitor = new jsMonitor_t( thisObj );
      JS_SetPrivate( cx, thisObj, monitor );
   }
   else
      JS_ReportError( cx, "Error allocating wlan monitor" );
   
   return JS_TRUE ;
}

bool initMonitorWLAN( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsMonitorWLANClass_,
                                  jsMonitorWLAN, 1,
                                  monitorWLANProperties_, 
                                  0, 0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      JS_ReportError( cx, "initializing WLAN monitor class\n" );
   
   return false ;
}
