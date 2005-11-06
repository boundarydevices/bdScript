/*
 * Module jsSniffWLAN.cpp
 *
 * This module defines the WLAN-sniffer routines
 * as declared and described in jsSniffWLAN.h
 *
 *
 * Change History : 
 *
 * $Log: jsSniffWLAN.cpp,v $
 * Revision 1.3  2005-11-06 00:49:39  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.2  2003/08/13 00:49:04  ericn
 * -fixed cancellation process
 *
 * Revision 1.1  2003/08/12 01:20:38  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsSniffWLAN.h"
#include <pthread.h>
#include "jsGlobals.h"
#include <unistd.h>
#include <stdio.h>
#include "sniffWLAN.h"
#include "codeQueue.h"

class jsSniffer_t : public sniffWLAN_t {
public:
   void onNew( ap_t &newOne );
};

static jsval     snifferObject_     = JSVAL_VOID ;
static jsval     snifferOnAP_       = JSVAL_VOID ;
static jsval     snifferOnComplete_ = JSVAL_VOID ;
static jsval     snifferAPObject_   = JSVAL_VOID ;
static pthread_t snifferThread_ = (pthread_t)0 ;

static unsigned     channel_ = 0 ;
static unsigned     numSeconds_ = 0 ;
static sniffWLAN_t *sniffer_ = 0 ;

static char const macFormat[] = {
   "%02X:%02X:%02X:%02X:%02X:%02X"
};

void jsSniffer_t :: onNew( ap_t &newOne )
{
   mutexLock_t lockExec( execMutex_ );
   JSObject * const jsObj = JS_NewObject( execContext_, 0, 0, 0 );
   if( jsObj )
   {
      snifferAPObject_ = OBJECT_TO_JSVAL( jsObj ); // root
      JSString *sSSID = JS_NewStringCopyZ( execContext_, newOne.ssid_ );
      if( sSSID )
      {
         if( JS_DefineProperty( execContext_, jsObj, "SSID", STRING_TO_JSVAL( sSSID ), 0, 0, JSPROP_ENUMERATE ) )
         {
            char cAP[sizeof(newOne.apMac_)*3];
            sprintf( cAP, macFormat, 
                     newOne.apMac_[0], newOne.apMac_[1], newOne.apMac_[2],
                     newOne.apMac_[3], newOne.apMac_[4], newOne.apMac_[5] );
            JSString *sAP = JS_NewStringCopyZ( execContext_, cAP );
            if( sAP )
            {
               if( JS_DefineProperty( execContext_, jsObj, "AP", STRING_TO_JSVAL( sAP ), 0, 0, JSPROP_ENUMERATE ) )
               {
                  char cBSS[sizeof(cAP)];
                  sprintf( cBSS, macFormat, 
                           newOne.bssid_[0], newOne.bssid_[1], newOne.bssid_[2],
                           newOne.bssid_[3], newOne.bssid_[4], newOne.bssid_[5] );
                  JSString *sBSS = JS_NewStringCopyZ( execContext_, cBSS );
                  if( sBSS )
                  {
                     if( JS_DefineProperty( execContext_, jsObj, "BSS", STRING_TO_JSVAL( sBSS ), 0, 0, JSPROP_ENUMERATE ) )
                     {
                        if( JS_DefineProperty( execContext_, jsObj, "channel", INT_TO_JSVAL( newOne.channel_ ), 0, 0, JSPROP_ENUMERATE ) 
                            &&
                            JS_DefineProperty( execContext_, jsObj, "signal", INT_TO_JSVAL( newOne.signal_ ), 0, 0, JSPROP_ENUMERATE ) 
                            &&
                            JS_DefineProperty( execContext_, jsObj, "noise", INT_TO_JSVAL( newOne.noise_ ), 0, 0, JSPROP_ENUMERATE ) 
                            &&
                            JS_DefineProperty( execContext_, jsObj, "WEP", INT_TO_JSVAL( newOne.requiresWEP_ ), 0, 0, JSPROP_ENUMERATE ) )
                        {
                           if( JSVAL_VOID != snifferOnAP_ )
                           {
                              if( queueSource( JSVAL_TO_OBJECT( snifferObject_ ), snifferOnAP_,
                                               "sniffWLAN", 1, &snifferAPObject_ ) )
                              {
                              }
                              else
                                 JS_ReportError( execContext_, "queueing sniff AP handler" );
                           }
                           else
                              JS_ReportError( execContext_, "No sniff AP handler" );
                        }
                        else
                           JS_ReportError( execContext_, "defining numeric members" );
                     }
                     else
                        JS_ReportError( execContext_, "defining BSS member" );
                  }
                  else
                     JS_ReportError( execContext_, "allocating BSS string" );
               }
               else
                  JS_ReportError( execContext_, "defining AP member" );
            }
            else
               JS_ReportError( execContext_, "allocating AP string" );
         }
         else
            JS_ReportError( execContext_, "setting SSID property" );
      }
      else
         JS_ReportError( execContext_, "allocating SSID string" );
   }
   else
      JS_ReportError( execContext_, "allocating AP object" );
}

static void* snifferThread( void *arg )
{
   if( 0 != sniffer_ )
   {
      sniffer_->sniff( channel_, numSeconds_ );
      if( JSVAL_VOID != snifferOnComplete_ )
      {
         if( queueSource( JSVAL_TO_OBJECT( snifferObject_ ), snifferOnComplete_, "sniffWLAN" ) )
         {
         }
         else
            printf( "Error queueing sniff completion handler\n" );
      }
      else
         printf( "No sniff complete handler\n" );
   }
   else
      printf( "No sniff object\n" );
   return 0 ;
}


static char const usage[] = {
   "usage: startSniffWLAN( { channel, numSeconds, onAP, onComplete } )"
};

static JSBool
jsStartSniff( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( (pthread_t) 0 == snifferThread_ )
   {
      if( ( 1 == argc ) && ( JSTYPE_OBJECT == JS_TypeOfValue( cx, argv[0] ) ) )
      {
         JSObject *rhObj = JSVAL_TO_OBJECT( argv[0] );
         jsval val ;
         
         if( JS_GetProperty( cx, rhObj, "channel", &val ) 
             &&
             JSVAL_IS_INT( val ) )
         {
            channel_ = JSVAL_TO_INT( val );
            if( ( 1 <= channel_ ) && ( 11 >= channel_ ) )
            {
               if( JS_GetProperty( cx, rhObj, "numSeconds", &val ) 
                   &&
                   JSVAL_IS_INT( val ) )
               {
                  numSeconds_ = JSVAL_TO_INT( val );
                  if( 0 < numSeconds_ )
                  {
                     if( JS_GetProperty( cx, rhObj, "onComplete", &val ) )
                     {
                        if( JSTYPE_FUNCTION == JS_TypeOfValue( cx, val ))
                           snifferOnComplete_ = val ;
                        else
                           printf( "onComplete is not a function\n" );
                     } // have onComplete handler
                     else
                     {
                        printf( "no onComplete handler specified\n" );
                     }
                        
            
                     if( JS_GetProperty( cx, rhObj, "onAP", &val ) )
                     {
                        if( JSTYPE_FUNCTION == JS_TypeOfValue( cx, val ))
                           snifferOnAP_ = val ;
                        else
                           printf( "onAP is not a function\n" );
                     } // have onAP handler
                     else
                     {
                        printf( "no onAP handler specified\n" );
                     }
            
                     snifferObject_ = OBJECT_TO_JSVAL( obj );
                     sniffWLAN_t * const newSniffer = new jsSniffer_t ;
                     if( newSniffer->sniffing() )
                     {
                        sniffer_ = newSniffer ;
                        if( 0 == pthread_create( &snifferThread_, 0, snifferThread, 0 ) )
                        {
                           *rval = JSVAL_TRUE ;
                           return JS_TRUE ;
                        }
                        else
                        {
                           snifferThread_ = 0; // just in case
                           JS_ReportError( cx, "starting sniffer thread" );
                        }
                     }
                     else
                        JS_ReportError( cx, "opening wlan for sniffing\n" );

                     delete newSniffer ;

                  }
                  else
                     JS_ReportError( cx, "invalid numSeconds %u\n", numSeconds_ );
               }
               else
                  JS_ReportError( cx, usage );
            }
            else
               JS_ReportError( cx, "invalid channel %u", channel_ );
         } // have channel
         else
            JS_ReportError( cx, "missing sniff channel\n" );
      }
      else
         JS_ReportError( cx, usage );
   }
   else
      JS_ReportError( cx, "Sniffer still running!\n" );
   return JS_TRUE ;
}

static JSBool
jsStopSniff( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( snifferThread_ && ( 0 != sniffer_ ) )
   {
      pthread_t tHandle = snifferThread_ ;
      snifferThread_ = 0 ;

printf( "cancelling sniffer\n" );
      sniffer_->cancel();

      void *exitStat ;
      pthread_join( tHandle, &exitStat );
printf( "done cancelling sniffer\n" );

      delete sniffer_ ;
      sniffer_ = 0 ;

      snifferObject_     = JSVAL_VOID ;
      snifferOnAP_       = JSVAL_VOID ;
      snifferOnComplete_ = JSVAL_VOID ;

      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "sniffer not running\n" );

   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"startSniffWLAN",  jsStartSniff,  0 },
    {"stopSniffWLAN",   jsStopSniff,   0 },
    {0}
};


bool initSniffWLAN( JSContext *cx, JSObject *glob )
{
   if( JS_DefineFunctions( cx, glob, _functions ) )
   {
      JS_AddRoot( cx, &snifferObject_ );
      JS_AddRoot( cx, &snifferOnAP_ );
      JS_AddRoot( cx, &snifferOnComplete_ );
      JS_AddRoot( cx, &snifferAPObject_ );
      return true ;
   }
   else
      return false ;
}


