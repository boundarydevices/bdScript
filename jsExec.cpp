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
 * Revision 1.58  2003-09-05 13:07:34  ericn
 * -added waitFor() routine to allow modal input
 *
 * Revision 1.57  2003/09/04 13:16:19  ericn
 * -made command line arguments available to script
 *
 * Revision 1.56  2003/09/01 23:35:31  ericn
 * -added method garbageCollect()
 *
 * Revision 1.55  2003/08/31 16:53:00  ericn
 * -added jsDir module
 *
 * Revision 1.54  2003/08/24 22:02:13  ericn
 * -added shutdown routine for jsProcess.cpp
 *
 * Revision 1.53  2003/08/24 17:30:31  ericn
 * -added child process control (sort of)
 *
 * Revision 1.52  2003/08/23 02:50:26  ericn
 * -added Javascript ping support
 *
 * Revision 1.51  2003/08/20 02:54:02  ericn
 * -added module jsMonWLAN
 *
 * Revision 1.50  2003/08/12 01:20:27  ericn
 * -added WLAN sniffing support
 *
 * Revision 1.49  2003/08/06 13:51:33  ericn
 * -modified to re-run same executable
 *
 * Revision 1.48  2003/08/03 03:25:31  ericn
 * -added flash module
 *
 * Revision 1.47  2003/07/30 20:25:26  ericn
 * -added jsMPEG reference
 *
 * Revision 1.46  2003/07/20 15:41:46  ericn
 * -removed debug msg
 *
 * Revision 1.45  2003/07/06 01:22:15  ericn
 * -added exit() support
 *
 * Revision 1.44  2003/07/03 14:08:00  ericn
 * -modified to release js file after compilation
 *
 * Revision 1.43  2003/07/03 03:17:47  ericn
 * -expanded JS mem
 *
 * Revision 1.42  2003/06/22 23:03:54  ericn
 * -removed closeCBM call
 *
 * Revision 1.41  2003/06/08 15:20:48  ericn
 * -added ffTest
 *
 * Revision 1.40  2003/06/07 16:41:52  ericn
 * -removed debug msg
 *
 * Revision 1.39  2003/06/06 03:38:57  ericn
 * -modified to use backtrace() to get a stack dump
 *
 * Revision 1.38  2003/05/10 19:14:10  ericn
 * -added closeCBM routine
 *
 * Revision 1.37  2003/05/09 04:27:04  ericn
 * -added CBM printer support
 *
 * Revision 1.36  2003/04/21 00:24:19  ericn
 * -added camera initialization
 *
 * Revision 1.35  2003/03/22 03:33:45  ericn
 * -added initialization for JPEG compression
 *
 * Revision 1.34  2003/03/12 02:56:56  ericn
 * -added module jsTransitions
 *
 * Revision 1.33  2003/03/04 14:45:18  ericn
 * -added jsFileIO module
 *
 * Revision 1.32  2003/02/27 03:51:09  ericn
 * -added exec routine
 *
 * Revision 1.31  2003/01/31 13:29:43  ericn
 * -added module jsURL
 *
 * Revision 1.30  2003/01/20 06:24:12  ericn
 * -Added module jsUse
 *
 * Revision 1.29  2003/01/06 04:30:53  ericn
 * -added exception handling
 *
 * Revision 1.28  2003/01/05 01:51:45  ericn
 * -stack dump exception handler
 *
 * Revision 1.27  2002/12/27 23:30:23  ericn
 * -added module jsTTY
 *
 * Revision 1.26  2002/12/16 19:44:26  tkisky
 * -leds and feedback
 *
 * Revision 1.25  2002/12/15 20:01:37  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.24  2002/12/15 00:10:10  ericn
 * -removed debug msgs
 *
 * Revision 1.23  2002/12/15 00:08:46  ericn
 * -removed debug msgs
 *
 * Revision 1.22  2002/12/15 00:07:58  ericn
 * -removed debug msgs
 *
 * Revision 1.21  2002/12/12 15:41:34  ericn
 * -added environment routines
 *
 * Revision 1.20  2002/12/10 04:48:30  ericn
 * -added module jsPopen
 *
 * Revision 1.19  2002/12/01 02:43:39  ericn
 * -changed queueCode() to root code, moved handler execution to pollCodeQueue()
 *
 * Revision 1.18  2002/11/30 18:52:57  ericn
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
#include <sys/stat.h>

/* include the JS engine API header */
#include "js/jsstddef.h"
#include "js/jsapi.h"
#include "relativeURL.h"
#include "jsHyperlink.h"
#include "jsExit.h"
#include "codeQueue.h"
#include "jsTimer.h"
#include "jsCurl.h"
#include "jsImage.h"
#include "jsJPEG.h"
#include "jsTransitions.h"
#include "jsGlobals.h"
#include "jsScreen.h"
#include "jsText.h"
#include "jsAlphaMap.h"
#include "jsTouch.h"
#include "jsMP3.h"
#include "audioQueue.h"
#include "jsVolume.h"
#include "jsBarcode.h"
#include "jsGpio.h"
#include "jsShell.h"
#include "jsButton.h"
#include "ccActiveURL.h"
#include "ccDiskCache.h"
#include "ccWorker.h"
#include "urlFile.h"
#include "jsPopen.h"
#include "jsEnviron.h"
#include "jsTCP.h"
#include "jsTTY.h"
#include "jsUse.h"
#include "jsURL.h"
#include "jsFileIO.h"
#include "jsCamera.h"
#include "jsCBM.h"
#include "jsMPEG.h"
#include "jsFlash.h"
#include "jsSniffWLAN.h"
#include "jsMonWLAN.h"
#include "jsPing.h"
#include "jsProcess.h"
#include "jsDir.h"

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
jsQueueCode( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   *rval = JSVAL_TRUE ;
   for( int arg = 0 ; arg < argc ; arg++ )
   {
      if( !queueUnrootedSource( obj, argv[arg], "queueCode" ) )
      {
         JS_ReportError( cx, "queuing code" );
         *rval = JSVAL_FALSE ;
      }
   }

   return JS_TRUE;
}

// returns true if execution should continue
static bool mainLoop( JSContext *cx )
{
   if( !pollCodeQueue( cx, 5000, 1 ) )
   {
      mutexLock_t lock( execMutex_ );
      JS_GC( cx );
   }
   return !( gotoCalled_ || execCalled_ || exitRequested_ );
}

static JSBool
jsWaitFor( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   *rval = JSVAL_TRUE ;
   if( ( 2 == argc ) 
       && 
       ( JSTYPE_OBJECT == JS_TypeOfValue( cx, argv[0] ) ) 
       && 
       ( JSTYPE_FUNCTION == JS_TypeOfValue( cx, argv[1] ) ) )
   {
      while( mainLoop( cx ) )
      {
         jsval functionReturn ;
         JSFunction *function = JS_ValueToFunction( cx, argv[1] );
         JSObject   *callObj ;
         JS_ValueToObject( cx, argv[0], &callObj );
         if( JS_CallFunction( cx, callObj, function, 0, NULL, &functionReturn ) )
         {
            if( JSVAL_FALSE != functionReturn )
            {
               break;
            }
         }
         else
            JS_ReportError( cx, "evaluating function from waitFor\n" );
      }
   }
   else
      JS_ReportError( cx, "Usage: waitFor( object, function );" );

   return JS_TRUE ;
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

static JSBool
jsGarbageCollect( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   JS_GC( cx );
   *rval = JSVAL_TRUE ;
   return JS_TRUE;
}

static JSFunctionSpec shell_functions[] = {
    {"queueCode",       jsQueueCode,      0},
    {"nanosleep",       jsNanosleep,      0},
    {"garbageCollect",  jsGarbageCollect, 0},
    {"waitFor",         jsWaitFor,        0},
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
   if( 2 <= argc )
   {
      // initialize the JS run time, and return result in rt
      JSRuntime * const rt = JS_NewRuntime(4L * 1024L * 1024L);
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
                     initJSJPEG( cx, glob );
                     initJSTransitions( cx, glob );
                     initJSMP3( cx, glob );
                     initJSAlphaMap( cx, glob );
                     initJSHyperlink( cx, glob );
                     initJSExit( cx, glob );
                     initJSVolume( cx, glob );
                     initJSBarcode( cx, glob );
                     initJSShell( cx, glob );
                     initJSButton( cx, glob );
                     initJSPopen( cx, glob );
                     initJSGpio( cx, glob );
                     initJSEnv( cx, glob );
                     initJSTCP( cx, glob );
                     initJSTTY( cx, glob );
                     initJSUse( cx, glob );
                     initJSURL( cx, glob );
                     initJSFileIO( cx, glob );
                     initJSCamera( cx, glob );
                     initJSCBM( cx, glob );
                     initJSMPEG( cx, glob );
                     initJSFlash( cx, glob );
                     initSniffWLAN( cx, glob );
                     initMonitorWLAN( cx, glob );
                     initPing( cx, glob );
                     initJSProcess( cx, glob );
                     initJSDir( cx, glob );

                     getCurlCache();

                     touchScreenThread_t *tsThread ;
                     if( !initJSTouch( tsThread, cx, glob ) )
                        tsThread = 0 ;

                     //
                     // start up audio output 
                     //
                     audioQueue_t &audioOut = getAudioQueue(); 

                     JSObject *sArgv = JS_NewArrayObject( cx, 0, NULL );
                     if( sArgv )
                     {
                        if( JS_DefineProperty( cx, glob, "argv", OBJECT_TO_JSVAL( sArgv ), 0, 0, JSPROP_ENUMERATE ) ) // root
                        {
                           for( int arg = 2 ; arg < argc ; arg++ )
                           {
                              JSString *sArg = JS_NewStringCopyZ( cx, argv[arg] );
                              jsval     vArg = ( 0 != sArg ) ? STRING_TO_JSVAL( sArg ) : JSVAL_NULL ;
                              JS_SetElement( cx, sArgv, arg-2, &vArg );
                           }
                           JS_DefineProperty( cx, glob, "argc", INT_TO_JSVAL( argc-2 ), 0, 0, JSPROP_ENUMERATE );
                        }
                     }

                     {
                        JSScript *script = 0 ;
                        
                        { // limit scope of urlFile
                           urlFile_t f( argv[1] );
                           if( f.isOpen() )
                           {
                              JSErrorReporter oldReporter = JS_SetErrorReporter( cx, myError );
   
                              script= JS_CompileScript( cx, glob, (char const *)f.getData(), f.getSize(), argv[1], 1 );
                              if( !script )
                                 fprintf( stderr, "Error compiling script %s\n", argv[1] );
                           }
                           else
                              fprintf( stderr, "Error opening url %s\n", argv[1] );
                        }
                        
                        if( script )
                        {
                           pushURL( argv[1] );
                           
                           jsval rval;
                           JSBool exec ;
                           {
                              mutexLock_t lock( execMutex_ );
                              exec = JS_ExecuteScript( cx, glob, script, &rval );
                              JS_DestroyScript( cx, script );
                              script = 0 ;
                           } // limit

                           if( exec )
                           {
                              unsigned numEvents = 0 ;

                              while( mainLoop( cx ) )
                                 ;
//                                 printf( "in main loop\n" );
                           }
                           else
                              fprintf( stderr, "exec error %s\n", argv[1] );
                              
                           popURL();
                        }
                     } // limit scope of urlFile

                     shutdownTTY();
                     shutdownGpio();

                     shutdownJSProcesses();
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
      fprintf( stderr, "Usage : jsExec url\n" );
 
   return 0;

}

#include <prinit.h>
#include <signal.h>
#include "hexDump.h"
#include <execinfo.h>

static struct sigaction sa;
static struct sigaction oldint;

void handler(int sig) 
{
   pthread_t me = pthread_self();
   fprintf( stderr, "got signal, stack == %p (id %x)\n", &sig, me );
   fprintf( stderr, "sighandler at %p\n", handler );

   unsigned long addr = (unsigned long)&sig ;
   unsigned long page = addr & ~0xFFF ; // 4K
   unsigned long size = page+0x1000-addr ;
   hexDumper_t dumpStack( &sig, size ); // just dump this page
   while( dumpStack.nextLine() )
      fprintf( stderr, "%s\n", dumpStack.getLine() );

   void *btArray[128];
   int btSize;
   
   fprintf( stderr, "########## Backtrace ##########\n"
                    "Number of elements in backtrace: %u\n", backtrace(btArray, sizeof(btArray) / sizeof(void *) ) );

   if (btSize > 0)
   {
      char** btSymbols = backtrace_symbols( btArray, btSize );
      if( btSymbols )
      {
         for (int i = btSize - 1; i >= 0; --i)
            fprintf( stderr, "%s\n", btSymbols[i] );
      }
   }

   fprintf( stderr, "Handler done.\n" );
   fflush( stderr );
   if( oldint.sa_handler )
      oldint.sa_handler( sig );

   exit( 1 );
}


int main( int argc, char *argv[] )
{
   // Initialize the sa structure
   sa.sa_handler = handler;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   
   // Set up the signal handler
   sigaction(SIGSEGV, &sa, NULL);

   char *exePath = argv[0];
   
   {
      char temp[80];
      sprintf( temp, "/proc/%d/exe", getpid() );
      struct stat st ;
      int stResult = stat( temp, &st );
      if( 0 == stResult )
      {
         exePath = new char [ st.st_size + 1 ];
         int numRead = readlink( temp, exePath, st.st_size + 1 );
         if( 0 < numRead )
         {
            argv[0] = exePath ;
         }
         else
            fprintf( stderr, "Error resolving path2:%m\n" );
      }
      else
      {
         fprintf( stderr, "Error resolving exe:%m\n" );
      }
   } // limit scope of temporaries

   printf( "main thread %s %p (id %x)\n", argv[1], &argc, pthread_self() );
   do
   {
      int result = PR_Initialize( prMain, argc, argv, 0 );
      if( gotoCalled_ )
      {
         argv[1] = (char *)gotoURL_.c_str();
         execv( argv[0], argv ); // start next
      }
      else if( execCalled_ )
      {
         system( execCmd_.c_str() );
         execv( argv[0], argv ); // start next
      }
      else
         return exitStatus_ ;
   } while( 1 );

   return 0 ;
}
