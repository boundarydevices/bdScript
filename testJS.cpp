/*
 * Program testJS.cpp
 *
 * This program is simply a compilation test of the Netscape
 * Javascript interpreter source and build
 *
 *
 * Change History : 
 *
 * $Log: testJS.cpp,v $
 * Revision 1.16  2002-11-30 02:01:14  ericn
 * -rewrote as jsExec minus touch and sound
 *
 * Revision 1.15  2002/11/30 00:30:49  ericn
 * -removed curlCache and curlThread modules
 *
 * Revision 1.14  2002/10/31 02:03:17  ericn
 * -added curl thread startup/shutdown
 *
 * Revision 1.13  2002/10/27 17:38:40  ericn
 * -added hyperlink and process calls
 *
 * Revision 1.12  2002/10/26 14:13:54  ericn
 * -removed debug stmt
 *
 * Revision 1.11  2002/10/25 04:48:39  ericn
 * -limited Javascript memory to 1MB
 *
 * Revision 1.10  2002/10/25 03:07:16  ericn
 * -removed debug statements
 *
 * Revision 1.9  2002/10/25 02:20:05  ericn
 * -added child process initialization calls
 *
 * Revision 1.8  2002/10/24 13:16:21  ericn
 * -added MP3, relative URL support
 *
 * Revision 1.7  2002/10/20 16:30:11  ericn
 * -added URL support
 *
 * Revision 1.6  2002/10/18 01:18:13  ericn
 * -added text and screen clear support
 *
 * Revision 1.5  2002/10/13 15:52:07  ericn
 * -added ref to image module
 *
 * Revision 1.4  2002/09/29 17:35:55  ericn
 * -added curlFile class
 *
 * Revision 1.3  2002/09/28 17:05:07  ericn
 * -changed copyright tag
 *
 * Revision 1.2  2002/09/28 17:04:16  ericn
 * -changed #include references
 *
 * Revision 1.1.1.1  2002/09/28 16:50:46  ericn
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
#include "js/jsstddef.h"
#include "js/jsapi.h"
#include "relativeURL.h"
#include "jsHyperlink.h"
#include "codeQueue.h"
#include "jsTimer.h"
#include "jsCurl.h"
#include "jsImage.h"
#include "jsGlobals.h"
#include "jsScreen.h"
#include "jsText.h"
#include "jsAlphaMap.h"
#include "jsTouch.h"
#include "jsVolume.h"
#include "jsBarcode.h"
#include "jsShell.h"
#include "jsButton.h"
#include "ccActiveURL.h"
#include "ccDiskCache.h"
#include "ccWorker.h"
#include "urlFile.h"

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
      JS_ReportError( cx, "Usage : nanosleep( seconds )\n" );
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

int prMain(int argc, char **argv)
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
                     initJSText( cx, glob );
                     initializeCodeQueue( cx, glob );
                     initJSCurl( cx, glob );
                     initJSImage( cx, glob );
                     initJSAlphaMap( cx, glob );
                     initJSHyperlink( cx, glob );
                     initJSVolume( cx, glob );
                     initJSBarcode( cx, glob );
                     initJSShell( cx, glob );
                     initJSButton( cx, glob );

                     getCurlCache();

                     urlFile_t f( argv[1] );
                     if( f.isOpen() )
                     {
                        pushURL( argv[1] );
         
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
                              unsigned numEvents = 0 ;

                              while( 1 )
                              {
                                 if( gotoCalled_ )
                                 {
                                    printf( "executing %s\n", gotoURL_.c_str() );
                                    break;
                                 }
                                 else 
                                 {
                                    bool doGC = ( 0 == ( ++numEvents % 10 ) );

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
                                       doGC = true ;
                                    
                                    if( doGC )
                                    {
                                       printf( "collectin' garbage\n" );
                                       mutexLock_t lock( execMutex_ );
                                       JS_GC( cx );
                                       printf( "done\n" );
                                    }
                                 }
                              }
                           }
                           else
                              fprintf( stderr, "exec error %s\n", argv[1] );
                        }
                        else
                           fprintf( stderr, "Error compiling script %s\n", argv[1] );
                           
                        popURL();
                     }
                     else
                        fprintf( stderr, "Error opening url %s\n", argv[1] );

                     stopBarcodeThread();

                     shutdownCurlWorkers();
                     shutdownCCDiskCache();
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

#include <prinit.h>

int main( int argc, char *argv[] )
{
   int result = PR_Initialize( prMain, argc, argv, 0 );
   if( gotoCalled_ )
   {
      printf( "executing %s\n", gotoURL_.c_str() );
      argv[1] = (char *)gotoURL_.c_str();
      execv( argv[0], argv ); // start next
   }

   return result ;
}
