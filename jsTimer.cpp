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
 * Revision 1.1  2002-10-27 17:42:08  ericn
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

struct timerParam_t {
   unsigned long milliseconds_ ;
   std::string   sourceCode_ ;
};

static void *interval( void *arg )
{
   timerParam_t const *param = (timerParam_t *)arg ;

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
         if( !queueSource( param->sourceCode_, "interval timer" ) )
            fprintf( stderr, "Error queueing code from interval timer\n" );
      }
      else
      {
         printf( "interval timer cancelled %d\n", result );
         break;
      }
   } while( 1 );

   delete param ;
   
   return 0 ;
}

static void *oneShot( void *arg )
{
   timerParam_t const *param = (timerParam_t *)arg ;

   unsigned long const ms = param->milliseconds_ ;
   struct timespec tspec ;
   tspec.tv_sec  = ms / 1000 ;
   tspec.tv_nsec = ( ms % 1000 ) * 1000000 ;

   struct timespec remaining ;

   int const result = nanosleep( &tspec, &remaining );
   if( 0 == result )
   {
      if( !queueSource( param->sourceCode_, "oneShot timer" ) )
         fprintf( stderr, "Error queueing code from oneShot timer\n" );
   }
   else
      printf( "oneShot cancelled %d\n", result );
   
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
      JSString *str = JS_ValueToString( cx, argv[1] );
      
      timerParam_t *param = new timerParam_t ;
      param->milliseconds_ = JSVAL_TO_INT( argv[0] );
      param->sourceCode_   = JS_GetStringBytes( str );

      pthread_t thread ;
      int create = pthread_create( &thread, 0, interval, param );
      if( 0 == create )
      {
         *rval = INT_TO_JSVAL( thread );
         return JS_TRUE ;
      }
      else
         delete param ;
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
      JSString *str = JS_ValueToString( cx, argv[1] );
      
      timerParam_t *param = new timerParam_t ;
      param->milliseconds_ = JSVAL_TO_INT( argv[0] );
      param->sourceCode_   = JS_GetStringBytes( str );

      pthread_t thread ;
      int create = pthread_create( &thread, 0, oneShot, param );
      if( 0 == create )
      {
         *rval = INT_TO_JSVAL( thread );
         return JS_TRUE ;
      }
      else
         delete param ;
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
         printf( "timer cancelled\n" );
         void *exitStat ;
         result = pthread_join( thread, &exitStat );
         if( 0 == result )
         {
            printf( "timer thread complete, status %ld\n", (int)exitStat );
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


