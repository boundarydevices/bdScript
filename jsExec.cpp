/*
 * Program testEvents.cpp
 *
 * This program is a minimal test program for testing 
 * the event-handling and code-queueing facilities of the
 * Mozilla Javascript interpreter
 *
 *
 * Change History : 
 *
 * $Log: jsExec.cpp,v $
 * Revision 1.2  2002-10-31 02:04:17  ericn
 * -added nanosleep, modified queueCode to include scope
 *
 * Revision 1.1  2002/10/27 17:42:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

/* include the JS engine API header */
#include "testEvents.h"
#include "js/jsstddef.h"
#include "js/jsapi.h"
#include "curlCache.h"
#include "relativeURL.h"
#include "jsHyperlink.h"
#include "codeQueue.h"
#include "jsTimer.h"
#include "jsCurl.h"
#include "curlThread.h"
#include "jsImage.h"
#include "jsGlobals.h"
#include "jsScreen.h"

static JSBool
global_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
               JSObject **objp)
{
    if ((flags & JSRESOLVE_ASSIGNING) == 0) {
        JSBool resolved;

        if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
            return JS_FALSE;
        if (resolved) {
            *objp = obj;
            return JS_TRUE;
        }
    }

    return JS_TRUE;
}


static JSClass global_class = {
    "global",                    JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,             JS_PropertyStub,
    JS_PropertyStub,             JS_PropertyStub,
    JS_EnumerateStandardClasses, (JSResolveOp) global_resolve,
    JS_ConvertStub,              JS_FinalizeStub
};

static JSBool
Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i, n;
    JSString *str;

    for (i = n = 0; i < argc; i++) {
        str = JS_ValueToString(cx, argv[i]);
        if (!str)
            return JS_FALSE;
        fprintf(stdout, "%s%s", i ? " " : "", JS_GetStringBytes(str));
    }
    n++;
    if (n)
        fputc('\n', stdout);
    return JS_TRUE;
}

static JSBool
jsQueueCode( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   *rval = JSVAL_TRUE ;
   for( int arg = 0 ; arg < argc ; arg++ )
   {
      JSString *str = JS_ValueToString(cx, argv[0]);
      if( str )
      {
         if( queueSource( obj,
                          std::string( JS_GetStringBytes( str ), JS_GetStringLength( str ) ), 
                          "queueCode" ) )
         {
            printf( "code queued\n" );
         }
         else
         {
            fprintf( stderr, "error queuing code\n" );
            *rval = JSVAL_FALSE ;
         }
      }
      else
         *rval = JSVAL_FALSE ;        
   }

   return JS_TRUE;
}

static JSBool
jsNanosleep( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   if( ( 1 == argc ) && JSVAL_IS_NUMBER( argv[0] ) )
   {
      double seconds ;
      JS_ValueToNumber( cx, argv[0], &seconds );
      struct timespec tv ;
      tv.tv_sec = (unsigned)( floor( seconds ) );
      tv.tv_nsec = (unsigned)( floor( fmod( seconds * 1000000000, 1000000000.0 ) ) );

//      JS_ReportError( cx, "nanosleep %f : %u.%lu seconds\n", seconds, tv.tv_sec, tv.tv_nsec );

      nanosleep( &tv, 0 );

      *rval = JSVAL_TRUE ;
   }
   else
   {
      JS_ReportError( cx, "Usage : nanosleep( ns )\n" );
      *rval = JSVAL_FALSE ;
   }

   return JS_TRUE;
}

static JSFunctionSpec shell_functions[] = {
    {"print",           Print,          0},
    {"queueCode",       jsQueueCode,    0},
    {"nanosleep",       jsNanosleep,    0},
    {0}
};


static void myError( JSContext *cx, const char *message, JSErrorReport *report)
{
   fprintf( stderr, "Error %s\n", message );
   fprintf( stderr, "file %s, line %u\n", report->filename, report->lineno );
}


/* main function sets up global JS variables, including run time,
 * a context, and a global object, then initializes the JS run time,
 * and creates a context. */

int main(int argc, char **argv)
{
   if( 2 == argc )
   {
      // initialize the JS run time, and return result in rt
      JSRuntime * const rt = JS_NewRuntime(1L * 1024L * 1024L);
      if( rt )
      {
         // create a context and associate it with the JS run time
         JSContext * const cx = JS_NewContext(rt, 8192);
         
         // if cx does not have a value, end the program here
         if( cx )
         {
            execContext_ = cx ;

            // create the global object here
            JSObject  *glob = JS_NewObject(cx, &global_class, NULL, NULL);
            if( 0 != glob )
            {
               // initialize the built-in JS objects and the global object
               if( JS_InitStandardClasses(cx, glob) )
               {
                  if( JS_DefineFunctions( cx, glob, shell_functions) )
                  {
                     initJSTimer( cx, glob );
                     initJSScreen( cx, glob );
                     initializeCodeQueue( cx, glob );
                     initJSCurl( cx, glob );
                     initJSImage( cx, glob );
                     startCurlThreads();

                     curlCache_t &cache = getCurlCache();
                     curlFile_t f( cache.get( argv[1], false ) );
                     if( f.isOpen() )
                     {
                        pushURL( f.getEffectiveURL() );
         
                        JSErrorReporter oldReporter = JS_SetErrorReporter( cx, myError );

                        JSScript *script= JS_CompileScript( cx, glob, (char const *)f.getData(), f.getSize(), argv[1], 1 );
                        if( script )
                        {
                           jsval rval; 
                           JSBool exec ;
                           {
                              mutexLock_t lock( execMutex_ );
                              exec = JS_ExecuteScript( cx, glob, script, &rval );
                              JS_DestroyScript( cx, script );
                           } // limit 


                           if( exec )
                           {
                              while( 1 )
                              {
                                 if( gotoCalled_ )
                                 {
                                    argv[2] = (char *)gotoURL_.c_str();
                                    argv[3] = 0 ;
                                    execv( argv[0], argv ); // start next
                                 }
                                 else 
                                 {
                                    JSObject *scope ;
                                    if( dequeueByteCode( script, scope, 5000 ) )
                                    {
                                       mutexLock_t lock( execMutex_ );
                                       exec = JS_ExecuteScript( cx, scope, script, &rval );
                                       if( !exec )
                                          fprintf( stderr, "error executing code\n" );
                                       JS_DestroyScript( cx, script );
                                    }
                                    else
                                    {
                                       printf( "script complete due to timeout\n" );
                                       break;
                                    }
                                 }
                              }
                           }
                           else
                              fprintf( stderr, "exec error %s\n", f.getEffectiveURL() );
                        }
                        else
                           fprintf( stderr, "Error compiling script %s\n", f.getEffectiveURL() );
                           
                        popURL();
                     }
                     else
                        fprintf( stderr, "Error opening url %s\n", argv[1] );

                     stopCurlThreads();
                  }
                  else
                     fprintf( stderr, "Error defining Javascript shell functions\n" );
               }
               else
                  fprintf( stderr, "Error creating Javascript builtins\n" );
            }
            else
               fprintf( stderr, "Error allocating Javascript global\n" );
   
            {
               mutexLock_t lock( execMutex_ );
               JS_DestroyContext( cx );
            }
   
         }
         else
            fprintf( stderr, "Error initializing Javascript context\n" );
      }
      else
         fprintf( stderr, "Error initializing Javascript runtime\n" );
   }
   else
      fprintf( stderr, "Usage : testEvents url\n" );
 
   return 0;

}

