/*
 * Module jsTimer.cpp
 *
 * This module defines the initialization routine
 * and Javascript support routines for timers, as
 * declared in jsTimer.h
 *
 *
 * Change History : 
 *
 * $Log: jsTimer.cpp,v $
 * Revision 1.5  2002-11-30 23:58:01  ericn
 * -rooted timer handlers
 *
 * Revision 1.4  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.3  2002/10/31 02:07:14  ericn
 * -modified to include scope in queueSource
 *
 * Revision 1.2  2002/10/27 17:48:57  ericn
 * -removed debug code
 *
 * Revision 1.1  2002/10/27 17:42:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsTimer.h"
#include <time.h>
#include <string>
#include <pthread.h>
#include "codeQueue.h"
#include "jsGlobals.h"

struct timerParam_t {
   JSObject     *scope_ ;
   unsigned long milliseconds_ ;
   jsval         sourceCode_ ;
};

static void *interval( void *arg )
{
   timerParam_t *param = (timerParam_t *)arg ;

   unsigned long const ms = param->milliseconds_ ;
   struct timespec tspec ;
   tspec.tv_sec  = ms / 1000 ;
   tspec.tv_nsec = ( ms % 1000 ) * 1000000 ;

   do {
      struct timespec interval = tspec ;
      struct timespec remaining ;
      int result = nanosleep( &interval, &remaining );
      if( 0 == result )
      {
         if( !queueSource( param->scope_, param->sourceCode_, "interval timer" ) )
            fprintf( stderr, "Error queueing code from interval timer\n" );
      }
      else
      {
         break;
      }
   } while( 1 );

   JS_RemoveRoot( execContext_, &param->sourceCode_ );

   delete param ;
   
   return 0 ;
}

static void *oneShot( void *arg )
{
   timerParam_t *param = (timerParam_t *)arg ;

   unsigned long const ms = param->milliseconds_ ;
   struct timespec tspec ;
   tspec.tv_sec  = ms / 1000 ;
   tspec.tv_nsec = ( ms % 1000 ) * 1000000 ;

   struct timespec remaining ;

   int const result = nanosleep( &tspec, &remaining );
   if( 0 == result )
   {
      if( !queueSource( param->scope_, param->sourceCode_, "oneShot timer" ) )
         fprintf( stderr, "Error queueing code from oneShot timer\n" );
   }
   else
      printf( "oneShot cancelled %d\n", result );

   JS_RemoveRoot( execContext_, &param->sourceCode_ );

   delete param ;
   return (void *)result ;
}

static JSBool
jsTimer( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 2 == argc ) 
       && 
       JSVAL_IS_INT( argv[0] )
       && 
       JSVAL_IS_STRING( argv[1] ) )
   {
      timerParam_t *param = new timerParam_t ;
      param->scope_        = obj ;
      param->milliseconds_ = JSVAL_TO_INT( argv[0] );
      param->sourceCode_   = argv[1];

      JS_AddRoot( execContext_, &param->sourceCode_ );
      pthread_t thread ;
      int create = pthread_create( &thread, 0, interval, param );
      if( 0 == create )
      {
         *rval = INT_TO_JSVAL( thread );
         return JS_TRUE ;
      }
      else
      {
         JS_RemoveRoot( execContext_, &param->sourceCode_ );
         delete param ;
      }
   }

   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}


static JSBool
jsOneShot( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 2 == argc ) 
       && 
       JSVAL_IS_INT( argv[0] )
       && 
       JSVAL_IS_STRING( argv[1] ) )
   {
      timerParam_t *param = new timerParam_t ;
      param->scope_        = obj ;
      param->milliseconds_ = JSVAL_TO_INT( argv[0] );
      param->sourceCode_   = argv[1];
      JS_AddRoot( execContext_, &param->sourceCode_ );

      pthread_t thread ;
      int create = pthread_create( &thread, 0, oneShot, param );
      if( 0 == create )
      {
         *rval = INT_TO_JSVAL( thread );
         return JS_TRUE ;
      }
      else
      {
         JS_RemoveRoot( execContext_, &param->sourceCode_ );
         delete param ;
      }
   }

   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}


static JSBool
jsCancelTimer( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       && 
       JSVAL_IS_INT( argv[0] ) )
   {
      pthread_t thread = JSVAL_TO_INT( argv[0] );
      int result = pthread_cancel( thread );
      if( 0 == result )
      {
         void *exitStat ;
         result = pthread_join( thread, &exitStat );
         if( 0 == result )
         {
            *rval = JSVAL_TRUE ;
            return JS_TRUE ;
         }
         else
            fprintf( stderr, "Error %d(%m) waiting for timer thread\n", result );
      }
      else
         fprintf( stderr, "Error %d(%m) cancelling timer\n", result );
   }
   
   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}


static JSFunctionSpec _functions[] = {
    {"timer",           jsTimer,          1 },
    {"oneShot",         jsOneShot,        1 },
    {"cancelTimer",     jsCancelTimer,    1 },
    {0}
};

bool initJSTimer( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}


