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
 * Revision 1.105  2008-12-02 00:22:20  ericn
 * use exec, not system to launch another program
 *
 * Revision 1.104  2008-10-16 00:09:25  ericn
 * [getFB()] Allow fbDev.cpp to read environment variable FBDEV to determine default FB
 *
 * Revision 1.103  2008-09-30 23:21:07  ericn
 * remove unused variable
 *
 * Revision 1.102  2008-09-09 17:42:10  ericn
 * [audio] Don't pre-open audioOutPoll
 *
 * Revision 1.101  2008-06-24 23:33:00  ericn
 * [jsCursor] Add support for Davinci HW cursor
 *
 * Revision 1.100  2008-06-11 18:13:46  ericn
 * -mainLoop() -- only garbage collect when something is active
 *
 * Revision 1.99  2008-01-04 23:31:15  ericn
 * -conditionally include GPIO stuff
 *
 * Revision 1.98  2007/08/08 17:13:49  ericn
 * -allow framebuffer device override with FRAMEBUFFER environment variable
 *
 * Revision 1.97  2007/07/29 19:18:17  ericn
 * -remove printf
 *
 * Revision 1.95  2007/07/07 00:25:52  ericn
 * -[bdScript] added mouse cursor and input handling
 *
 * Revision 1.94  2007/05/18 19:38:36  ericn
 * -add watchdog support
 *
 * Revision 1.93  2006/02/13 21:17:38  ericn
 * -remove jsKernel reference
 *
 * Revision 1.92  2007/01/12 00:02:51  ericn
 * -show library addresses in stack dump
 *
 * Revision 1.91  2006/12/01 18:39:37  tkisky
 * -better stack dump on error
 *
 * Revision 1.90  2006/10/29 21:57:12  ericn
 * -add jsUsblp initialization
 *
 * Revision 1.89  2006/10/05 14:40:51  ericn
 * -add detach flag to cmd line
 *
 * Revision 1.88  2006/08/16 21:11:10  ericn
 * -allow GPIO support
 *
 * Revision 1.87  2006/05/14 14:42:19  ericn
 * -add jsPNG, audioOutPoll
 *
 * Revision 1.86  2005/12/11 16:02:30  ericn
 * -
 *
 * Revision 1.85  2005/11/07 15:36:12  ericn
 * -fix conditional wlan
 *
 * Revision 1.84  2005/11/06 20:42:11  ericn
 * -CONFIG_XYZ == 1
 *
 * Revision 1.83  2005/11/06 20:26:43  ericn
 * -conditional Monitor WLAN
 *
 * Revision 1.82  2005/11/06 17:32:24  ericn
 * -added modularity based on kernel config params
 *
 * Revision 1.81  2004/12/28 03:46:43  ericn
 * -additional cleanup
 *
 * Revision 1.80  2004/09/27 04:51:05  ericn
 * -move md5 routine to its' own module
 *
 * Revision 1.79  2004/09/25 14:17:02  ericn
 * -made global_class global
 *
 * Revision 1.78  2004/07/04 21:32:38  ericn
 * -added alignment constants (generally useful)
 *
 * Revision 1.77  2004/06/28 02:57:09  ericn
 * -add starUSB support
 *
 * Revision 1.76  2004/05/22 18:02:14  ericn
 * -removed reference to prinit.h
 *
 * Revision 1.75  2004/05/07 13:32:17  ericn
 * -added barcode generation module
 *
 * Revision 1.74  2004/05/05 03:19:20  ericn
 * -added jsPrinter initialization
 *
 * Revision 1.73  2004/04/18 16:11:21  ericn
 * -allow flashVar on BD2004
 *
 * Revision 1.72  2004/03/27 20:22:48  ericn
 * -added jsSerial
 *
 * Revision 1.71  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.70  2004/02/07 12:14:13  ericn
 * -added flashVar global
 *
 * Revision 1.69  2004/02/03 06:12:22  ericn
 * -saves base url for scripts
 *
 * Revision 1.68  2004/01/02 23:36:56  ericn
 * -allow shebang
 *
 * Revision 1.67  2004/01/01 20:31:44  ericn
 * -no wait for goto in main body
 *
 * Revision 1.66  2003/12/27 18:38:31  ericn
 * -added pollTimer reference
 *
 * Revision 1.65  2003/11/30 16:45:53  ericn
 * -kernel and JFFS2 upgrade support
 *
 * Revision 1.64  2003/11/28 14:11:18  ericn
 * -added method pollStat(), fixed backtrace
 *
 * Revision 1.63  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.62  2003/11/22 21:02:37  ericn
 * -made code queue a pollHandler_t
 *
 * Revision 1.61  2003/09/10 04:56:30  ericn
 * -Added UDP support
 *
 * Revision 1.60  2003/09/09 03:58:34  ericn
 * -modified to supply default environment variables
 *
 * Revision 1.59  2003/09/06 19:50:13  ericn
 * -added md5 routine
 *
 * Revision 1.58  2003/09/05 13:07:34  ericn
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

#include "config.h"
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
#include "jsPNG.h"
#include "jsGlobals.h"
#include "jsScreen.h"
#include "jsText.h"
#include "jsAlphaMap.h"
#include "jsTouch.h"

#if CONFIG_JSBARCODE == 1
#include "jsBarcode.h"
#include "jsBCWidths.h"
#endif

#ifdef KERNEL_PXA_GPIO
#include "jsGpio.h"
#endif

#if CONFIG_JSCAIRO == 1
#include "jsCairo.h"
#endif

// #include "jsShell.h"
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

#if CONFIG_JSMONITORWLAN == 1
#include "jsSniffWLAN.h"
#include "jsMonWLAN.h"
#endif

#if KERNEL_INPUT == 1
#include "jsInput.h"
#endif

#include "jsPing.h"
#include "jsProcess.h"
#include "jsDir.h"
#include "jsUDP.h"
#include "pollTimer.h"
#include "memFile.h"
#include "debugPrint.h"
#include "jsBitmap.h"
#include "fbDev.h"
#include "jsSerial.h"
#include "jsFlashVar.h"
#include "jsMD5.h"
#include <fcntl.h>
#include "jsUsblp.h"

#include "touchPoll.h"

#ifdef KERNEL_SOUND
   #define CONFIG_JSMP3 1
   #define CONFIG_JSFLASH 1
   #ifdef KERNEL_FB_SM501YUV 
      #define CONFIG_JSMPEG 1
   #endif
#else
   #undef CONFIG_JSMP3
   #undef CONFIG_JSFLASH
   #undef CONFIG_JSMPEG
#endif

#ifdef CONFIG_JSMP3
   #include "audioQueue.h"
   #include "audioOutPoll.h"
   #include "jsVolume.h"
   #include "jsMP3.h"
   
   #ifdef CONFIG_JSMPEG
      #include "jsMPEG.h"
   #endif
   
   #ifdef CONFIG_JSFLASH
      #include "jsFlash.h"
   #endif
#endif

#if CONFIG_JSCAMERA == 1
#include "jsCamera.h"
#else
#endif 

#if CONFIG_JSCBM == 1
#include "jsCBM.h"
#endif

#if CONFIG_JSPRINTER == 1
#include "jsPrinter.h"
#endif

#if CONFIG_JSSTARUSB == 1
#include "jsStarUSB.h"
#endif

#if (defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)) \
    || \
    (defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1))
#include "jsCursor.h"
#endif

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


JSClass global_class = {
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
   for( uintN arg = 0 ; arg < argc ; arg++ )
   {
      if( !queueUnrootedSource( obj, argv[arg], "queueCode" ) )
      {
         JS_ReportError( cx, "queuing code" );
         *rval = JSVAL_FALSE ;
      }
   }

   return JS_TRUE;
}

static int fdWatchdog = -1 ;

// returns true if execution should continue
static bool mainLoop( pollHandlerSet_t &polls,
                      JSContext        *cx )
{
   if( !( gotoCalled_ || execCalled_ || exitRequested_ ) )
   {
      static unsigned iterations = 0 ;
      bool active = polls.poll( 5000 );
      if( active ){
         iterations++ ;
      }
      else if( 1023 < iterations ){
         fprintf( stderr, "garbage-collect..." );
         mutexLock_t lock( execMutex_ );
         JS_GC( cx );
         fprintf( stderr, "done\n" );
         iterations = 0 ;
      }
   }

   if( 0 <= fdWatchdog )
      write( fdWatchdog, "1", 1 );

   return !( gotoCalled_ || execCalled_ || exitRequested_ );
}

pollHandlerSet_t pollHandlers_ ;

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
      while( mainLoop( pollHandlers_, cx ) )
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

static JSBool
jsPollStat( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   printf( "%u poll handlers installed\n", pollHandlers_.numHandlers() );
   for( unsigned i = 0 ; i < pollHandlers_.numHandlers(); i++ )
   {
      pollHandler_t *handler = pollHandlers_[i];
      printf( "[%u] == %p, fd %d, mask 0x%X:", i, handler, handler->getFd(), handler->getMask() );
      unsigned short const mask = handler->getMask();
      if( mask & POLLIN )  printf( "R" ); else printf( "r" );
      if( mask & POLLOUT ) printf( "W" ); else printf( "w" );
      if( mask & POLLERR ) printf( "E" ); else printf( "e" );
      if( pollHandlers_.isDeleted(i) )
         printf( "   deleted!" );
      printf( "\n" );
   }

   *rval = JSVAL_TRUE ;
   return JS_TRUE;
}

static JSBool
jsWdEnable( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   if( 0 > fdWatchdog ){
	fdWatchdog = open( "/dev/watchdog", O_WRONLY );
	if( 0 <= fdWatchdog ){
		fcntl( fdWatchdog, F_SETFD, FD_CLOEXEC );
	}
	else {
		JS_ReportError( cx, "watchdog: %m\n" );
	}
   }

   *rval = (0 <= fdWatchdog) ? JSVAL_TRUE : JSVAL_FALSE ;
   return JS_TRUE;
}

static JSFunctionSpec shell_functions[] = {
    {"queueCode",       jsQueueCode,      0},
    {"nanosleep",       jsNanosleep,      0},
    {"garbageCollect",  jsGarbageCollect, 0},
    {"pollStat",        jsPollStat,       0},
    {"waitFor",         jsWaitFor,        0},
    {"wdEnable",        jsWdEnable,       0},
    {0}
};

static void myError( JSContext *cx, const char *message, JSErrorReport *report)
{
   fprintf( stderr, "Error %s\n", message );
   fprintf( stderr, "file %s, line %u\n", report->filename, report->lineno );
}

struct namedConstant_t {
   char const *name ;
   int         value ;
};

static namedConstant_t constants_[] = {
   { "centerHorizontal",  
     alignCenterHorizontal },
   { "alignRight",
     alignRight },
   { "alignLeft",
     0 },
   { "alignTop",
     0 },
   { "alignBottom",
     alignBottom },
   { "centerVertical",
     alignCenterVertical },
   { 0, 0 }
};

/* main function sets up global JS variables, including run time,
 * a context, and a global object, then initializes the JS run time,
 * and creates a context. */

int prMain(int argc, char **argv, bool )
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
                  namedConstant_t const *nextConst = constants_ ;
                  for( ; nextConst->name ; nextConst++ )
                  {
                     JS_DefineProperty( cx, glob, nextConst->name, 
                                        INT_TO_JSVAL( nextConst->value ), 
                                        0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
                  }

                  getTimerPoll( pollHandlers_ );
                  initJSTimer( cx, glob );
                  initJSScreen( cx, glob );
                  initJSText( cx, glob );
                  initializeCodeQueue( pollHandlers_, cx, glob );
                  initJSCurl( cx, glob );
                  initJSImage( cx, glob );
                  initJSJPEG( cx, glob );
                  initJSPNG( cx, glob );
                  initJSAlphaMap( cx, glob );
                  initJSBitmap( cx, glob );
                  initJSHyperlink( cx, glob );
                  initJSExit( cx, glob );
//                  initJSShell( cx, glob );
                  initJSPopen( cx, glob );
                  initJSMD5( cx, glob );
                  initJSEnv( cx, glob );
                  initJSTCP( cx, glob );
                  initJSTTY( cx, glob );
                  initJSUse( cx, glob );
                  initJSURL( cx, glob );
                  initJSFileIO( cx, glob );
#if KERNEL_INPUT == 1
                  initJSInput( cx, glob );
#endif

#if CONFIG_JSMONITORWLAN == 1
                  initSniffWLAN( cx, glob );
                  initMonitorWLAN( cx, glob );
#endif

                  initPing( cx, glob );
                  initJSProcess( cx, glob );
                  initJSDir( cx, glob );
                  initJSUDP( cx, glob );
                  initJSSerial( cx, glob );
                  initJSFlashVar( cx, glob );
                  initJSButton( cx, glob );
                  initJSTouch( cx, glob );

#if CONFIG_JSBARCODE == 1 
                  initJSBarcode( cx, glob );
                  initJSBCWidths( cx, glob );
#endif

#ifdef KERNEL_PXA_GPIO
                  initJSGpio( cx, glob );
#endif                  

#if CONFIG_JSCAIRO == 1
                  initJSCairo( cx, glob );
#endif                  

#if (defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)) \
    || \
    (defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1))
                  initJSCursor( cx, glob );
#endif

#ifdef CONFIG_JSMP3
                  initJSMP3( cx, glob );
                  initJSVolume( cx, glob );
   #if defined(CONFIG_JSMPEG) && (1==CONFIG_JSMPEG)
                  initJSMPEG( cx, glob );
   #endif
   
   #ifdef CONFIG_JSFLASH
                  initJSFlash( cx, glob );
   #endif
                  //
                  // start up audio output 
                  //
//                  audioQueue_t &audioOut = 
                  (void)getAudioQueue(); 
//                  audioOutPoll_t::get(pollHandlers_);
#else
#endif

#if CONFIG_JSCAMERA == 1
                  initJSCamera( cx, glob );
#endif 
                  
#if CONFIG_JSPRINTER == 1
                  initPrinter( cx, glob );
#endif

#if CONFIG_JSCBM == 1
                  initJSCBM( cx, glob );
#endif
                  
#if CONFIG_JSSTARUSB == 1
                  initJSStarUSB( cx, glob );
#endif

                  initJSUsbLp( cx, glob );

                  getCurlCache();

                  JSObject *sArgv = JS_NewArrayObject( cx, 0, NULL );
                  if( sArgv )
                  {                     
                     if( JS_DefineProperty( cx, glob, "argv", OBJECT_TO_JSVAL( sArgv ), 0, 0, JSPROP_ENUMERATE ) ) // root
                     {
                        // JSErrorReporter oldReporter = 
                        JS_SetErrorReporter( cx, myError );

                        JSScript *script = 0 ;

                        if( ( 2 <= argc ) && ( '-' == argv[1][0] ) )
                        {
                           char fullpath[512];
                           argc-- ; argv++ ;
                           std::string url( "file://" );
                           url += realpath( argv[1], fullpath );
                           pushURL( url );
                           memFile_t fIn( argv[1] );
                           if( fIn.worked() )
                           {
                              char const *start = (char const *)fIn.getData();
                              char const *const end = start + fIn.getLength();
                              while( start < end )
                              {
                                 if( '\n' == *start++ )
                                    break;
                              }

                              if( start < end )
                                 script= JS_CompileScript( cx, glob, start, end-start, argv[1], 1 );
                           }
                        }
                        else
                        { // limit scope of urlFile
                           urlFile_t f( argv[1] );
                           if( f.isOpen() )
                           {
                              script= JS_CompileScript( cx, glob, (char const *)f.getData(), f.getSize(), argv[1], 1 );
                           }
                           else 
                              fprintf( stderr, "Error opening url %s\n", argv[1] );
                        }
                        
                        if( script )
                        {
                           for( int arg = 2 ; arg < argc ; arg++ )
                           {
                              JSString *sArg = JS_NewStringCopyZ( cx, argv[arg] );
                              jsval     vArg = ( 0 != sArg ) ? STRING_TO_JSVAL( sArg ) : JSVAL_NULL ;
                              JS_SetElement( cx, sArgv, arg-2, &vArg );
                           }
                           JS_DefineProperty( cx, glob, "argc", INT_TO_JSVAL( argc-2 ), 0, 0, JSPROP_ENUMERATE );
   
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
                              while( mainLoop( pollHandlers_, cx ) )
                                 ;
                           }
                           else
                              fprintf( stderr, "exec error %s\n", argv[1] );
                              
                           popURL();
                        }
                        else
                           fprintf( stderr, "Error compiling script %s\n", argv[1] );
                     }
                  }

                  shutdownTTY();

#if CONFIG_JSCAIRO == 1
                  shutdownJSCairo();
#endif

#ifdef KERNEL_PXA_GPIO
                  shutdownGpio();
#endif

                  shutdownJSProcesses();
                  shutdownCurlWorkers();
                  shutdownCCDiskCache();
                  shutdownTouch();
                  abortCodeQueue();

#if CONFIG_JSMP3 == 1
                  audioQueue_t::shutdown();
#endif 
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
 
   if( 0 <= fdWatchdog )
      close( fdWatchdog );

   return 0;

}

#include <signal.h>
#include "hexDump.h"
#include <execinfo.h>

static struct sigaction sa;
static struct sigaction oldint;

void dumparea(unsigned int* p, int size)
{
	fprintf( stderr, "%p ",p);
	while (size > 0) {
		fprintf(stderr, "%08x ",*p);
		p++;
		size -= 4;
		if ( (((int)p)&0xf)==0) fprintf(stderr,"\n%p ",p);
	}
	fprintf( stderr, "\n");
}
struct sigcontext1 {
	unsigned long trap_no;
	unsigned long error_code;
	unsigned long oldmask;
	unsigned long arm_r0;
	unsigned long arm_r1;
	unsigned long arm_r2;
	unsigned long arm_r3;
	unsigned long arm_r4;
	unsigned long arm_r5;
	unsigned long arm_r6;
	unsigned long arm_r7;
	unsigned long arm_r8;
	unsigned long arm_r9;
	unsigned long arm_r10;
	unsigned long arm_fp;
	unsigned long arm_ip;
	unsigned long arm_sp;
	unsigned long arm_lr;
	unsigned long arm_pc;
	unsigned long arm_cpsr;
	unsigned long fault_address;
};

struct ucontext1 {
        unsigned long     uc_flags;
        struct ucontext  *uc_link;
        stack_t           uc_stack;
        struct sigcontext1 uc_mcontext;
};

void handler(int sig,siginfo_t* si,void* p) 
{
   pthread_t me = pthread_self();
   fprintf( stderr, "got signal %i, si=%p p=%p, stack == %p\n", sig, si,p,&me );
//   fprintf( stderr, "pid=%x, uid=%x\n",si->si_pid,si->si_uid); //pid not part of segv union
   fprintf( stderr, "sighandler at %p\n", handler );
   fprintf( stderr, "Failed access %p\n", (si->si_addr));
   struct ucontext1* pu = (struct ucontext1*)p;
   struct sigcontext1* uc = &pu->uc_mcontext;
   fprintf( stderr,"uc_mcontext:\n");
   fprintf( stderr,"r0:%08lx r1:%08lx r2:%08lx r3:%08lx\n",uc->arm_r0,uc->arm_r1,uc->arm_r2,uc->arm_r3);
   fprintf( stderr,"r4:%08lx r5:%08lx r6:%08lx r7:%08lx\n",uc->arm_r4,uc->arm_r5,uc->arm_r6,uc->arm_r7);
   fprintf( stderr,"r8:%08lx r9:%08lx sl:%08lx fp:%08lx\n",uc->arm_r8,uc->arm_r9,uc->arm_r10,uc->arm_fp);
   fprintf( stderr,"ip:%08lx sp:%08lx lr:%08lx pc:%08lx\n",uc->arm_ip,uc->arm_sp,uc->arm_lr,uc->arm_pc);
   fprintf( stderr,"cpsr:%08lx fault:%08lx\n",uc->arm_cpsr,uc->fault_address);

   FILE *fMaps = fopen( "/proc/self/maps", "rt" );
   if( fMaps ){
      char temp[132];
      while( 0 != fgets( temp,sizeof(temp)-1,fMaps ) )
         printf( "%s\n", temp );
      fclose( fMaps );
   }

   fprintf( stderr,"siginfo:\n");
   dumparea((unsigned int *)si,sizeof(siginfo_t));
   fprintf( stderr,"ucontext:\n");
   dumparea((unsigned int *)p,sizeof(struct ucontext));
   fprintf( stderr,"stack:\n");
   dumparea((unsigned int *)(uc->arm_sp), ((uc->arm_sp|0xfff)+1) - uc->arm_sp);

   unsigned long addr = (unsigned long)&sig ;
   unsigned long page = addr & ~0xFFF ; // 4K
   unsigned long size = page+0x1000-addr ;
   hexDumper_t dumpStack( &sig, size ); // just dump this page
   while( dumpStack.nextLine() )
      fprintf( stderr, "%s\n", dumpStack.getLine() );

   void *btArray[128];
   int const btSize = backtrace(btArray, sizeof(btArray) / sizeof(void *) );

   fprintf( stderr, "########## Backtrace ##########\n"
                    "Number of elements in backtrace: %u\n", btSize );

   if (btSize > 0)
      backtrace_symbols_fd( btArray, btSize, fileno(stdout) );

   fprintf( stderr, "Handler done.\n" );
   fflush( stderr );
   if (oldint.sa_flags & SA_SIGINFO) {
	if (oldint.sa_sigaction) oldint.sa_sigaction(sig,si,p);
   } else {
        if( oldint.sa_handler ) oldint.sa_handler( sig );
   }

   exit( 1 );
}

static char const *const requiredEnvVars[] = {
   "TSLIB_CONFFILE",
   "TSLIB_PLUGINDIR",
   "TSLIB_CALIBFILE",
   "CURLTMPSIZE",
};

static char const *const defaultEnvVars[] = {
   "/etc/ts.conf",
   "/share/ts/plugins",
   "/etc/ts.calibrate",
   "4000000"
};

#include "macros.h"

static bool allowCore = (0!=getenv("ALLOWCORE"));

int main( int argc, char *argv[] )
{
   if( 2 <= argc )
   {
      // Initialize the sa structure
      if( !allowCore ){
         sa.sa_sigaction = handler;
         sigemptyset(&sa.sa_mask);
         sa.sa_flags = SA_SIGINFO;
         // Set up the signal handler
         sigaction(SIGSEGV, &sa, NULL);
      }

      getFB();
   
      for( unsigned i = 0 ; i < dim( requiredEnvVars ); i++ )
      {
         char const *env = getenv( requiredEnvVars[i] );
         if( 0 == env )
         {
            setenv( requiredEnvVars[i], defaultEnvVars[i], 0 );
         }
      }
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

      char **nativeArgs = new char *[argc+1];
      memset( nativeArgs, 0, (argc+1)*sizeof(nativeArgs[0]) );
      bool detach = false ;
      int numArgs = 0 ;

      { // limit scope
         for( int arg = 0 ; arg < argc ; arg++ ){
            if( 0 == strcasecmp( "-d", argv[arg] ) ){
               detach = true ;
            }
            else
               nativeArgs[numArgs++] = argv[arg];
         }
      }

      if( detach ){
         printf( "detach from console\n" );
         for( int i = 0 ; i < 3 ; i++ )
            close( i );
         int fdNull = open( "/dev/null", O_RDWR );
         if( 0 <= fdNull ){
            for( int i = 0 ; i < 3 ; i++ )
               dup(fdNull);
         }
         else
            perror( "/dev/null" );
      }

      do
      {
         // int result = 
         prMain( numArgs, nativeArgs, detach );
         if( gotoCalled_ )
         {
            argv[1] = (char *)gotoURL_.c_str();
            execv( argv[0], argv ); // start next
         }
         else if( execCalled_ )
         {
            char **cmdline = new char *[execCmdArgs_.size()+1];
            char **nextCmd = cmdline ;
            for( unsigned i = 0 ; i < execCmdArgs_.size(); i++ ){
               *nextCmd++ = (char *)execCmdArgs_[i].c_str();
            }
            *nextCmd = 0 ;
            execvp( cmdline[0], cmdline ); // launch program
         }
         else
         {
            return exitStatus_ ;
         }
      } while( 1 );
   }
   else
      fprintf( stderr, 
               "Usage : jsExec [-d] url\n" 
               "-d      detach from stdin/stdout/stderr\n" );
   
   return 0 ;
}
