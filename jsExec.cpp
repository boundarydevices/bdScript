/*
 * Program jsExec.cpp
 *
 * This program is a Javascript shell of sorts, 
 * which executes the URL specified on the command
 * line.
 *
 *
 * Change History : 
 *
 * $Log: jsExec.cpp,v $
 * Revision 1.18  2002-11-30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.17  2002/11/30 16:33:12  ericn
 * -limit scope of urlFile
 *
 * Revision 1.16  2002/11/30 00:31:48  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.15  2002/11/21 14:04:27  ericn
 * -preliminary button code
 *
 * Revision 1.14  2002/11/17 23:08:50  ericn
 * -changed the name
 *
 * Revision 1.13  2002/11/17 22:24:45  ericn
 * -reworked goto, added jsShell
 *
 * Revision 1.12  2002/11/17 16:08:33  ericn
 * -modified to initialize NSPR
 *
 * Revision 1.11  2002/11/17 03:31:48  ericn
 * -temporary while checkin garbage collection
 *
 * Revision 1.10  2002/11/17 03:17:11  ericn
 * -added garbage collection, got rid of timeout
 *
 * Revision 1.9  2002/11/17 00:51:34  ericn
 * -Added Javascript barcode support
 *
 * Revision 1.8  2002/11/14 13:13:17  ericn
 * -added volume module
 *
 * Revision 1.7  2002/11/07 02:13:05  ericn
 * -added audioQueue calls
 *
 * Revision 1.6  2002/11/05 15:13:46  ericn
 * -added MP3 support (headers anyway)
 *
 * Revision 1.5  2002/11/03 15:38:20  ericn
 * -added touch screen, hyperlink support
 *
 * Revision 1.4  2002/11/02 18:36:09  ericn
 * -added alphaMap support
 *
 * Revision 1.3  2002/11/02 04:10:43  ericn
 * -added jsText initialization
 *
 * Revision 1.2  2002/10/31 02:04:17  ericn
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
#include "jsMP3.h"
#include "audioQueue.h"
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
      if( queueSource( obj, argv[arg], "queueCode" ) )
      {
         printf( "code queued\n" );
      }
      else
      {
         fprintf( stderr, "error queuing code\n" );
         *rval = JSVAL_FALSE ;
      }
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
                     initJSMP3( cx, glob );
                     initJSAlphaMap( cx, glob );
                     initJSHyperlink( cx, glob );
                     initJSVolume( cx, glob );
                     initJSBarcode( cx, glob );
                     initJSShell( cx, glob );
                     initJSButton( cx, glob );

                     getCurlCache();

                     touchScreenThread_t *tsThread ;
                     if( !initJSTouch( tsThread, cx, glob ) )
                        tsThread = 0 ;
                     
                     //
                     // start up audio output 
                     //
                     audioQueue_t &audioOut = getAudioQueue(); 

                     {
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
                     } // limit scope of urlFile

                     stopBarcodeThread();

                     shutdownCurlWorkers();
                     shutdownCCDiskCache();

                     audioQueue_t::shutdown();

                     if( tsThread )
                        delete tsThread ;
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
