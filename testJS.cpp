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
 * Revision 1.9  2002-10-25 02:20:05  ericn
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

/* include the JS engine API header */
#include "js/jsstddef.h"
#include "js/jsapi.h"
#include "jsCurl.h"
#include "jsImage.h"
#include "jsText.h"
#include "jsScreen.h"
#include "jsMP3.h"
#include "jsURL.h"
#include "curlCache.h"
#include "relativeURL.h"
#include "childProcess.h"

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
urlExec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i ;
    
    curlCache_t &cache = getCurlCache();

    for (i = 0; i < argc; i++) {
        JSString *str = JS_ValueToString(cx, argv[i]);
        if( str )
        {
           char const *cURL = JS_GetStringBytes( str );
           curlFile_t f( cache.get( cURL, false ) );
           if( f.isOpen() )
           {
              pushURL( f.getEffectiveURL() );
               JSScript *script= JS_CompileScript( cx, JS_GetGlobalObject( cx ), (char const *)f.getData(), f.getSize(), cURL, 1 );
               if( script )
               {
                  printf( "compiled script for %s\n", cURL );
                  jsval rval; 
                  JSBool exec = JS_ExecuteScript( cx, JS_GetGlobalObject( cx ), script, &rval );
                  if( exec )
                  {
                     printf( "executed\n" );
                  }
                  else
                     printf( "exec error\n" );
                  JS_DestroyScript( cx, script );
               }
               else
                  printf( "Error compiling script\n" );
               
               popURL();
           }
           else
              fprintf( stderr, "Error opening %s\n", cURL );
        }
        else
           return JS_FALSE;
    }
    return JS_TRUE;
}

static JSFunctionSpec shell_functions[] = {
    {"print",           Print,          0},
    {0}
};

static JSFunctionSpec shell_functions2[] = {
    {"urlExec",         urlExec,        0},
    {0}
};

/* main function sets up global JS variables, including run time,
 * a context, and a global object, then initializes the JS run time,
 * and creates a context. */

int main(int argc, char **argv)
{
   // initialize the JS run time, and return result in rt
   JSRuntime * const rt = JS_NewRuntime(2L * 1024L * 1024L);
 
   printf( "allocated runtime\n" );
   // if rt does not have a value, end the program here
   if (!rt)
     return 1;
 
   // create a context and associate it with the JS run time
   JSContext * const cx = JS_NewContext(rt, 8192);
 
   printf( "allocated context\n" );
   // if cx does not have a value, end the program here
   if (cx == NULL)
     return 1;
 
   // create the global object here
   JSObject  *glob = JS_NewObject(cx, &global_class, NULL, NULL);
 
   printf( "created glob\n" );
   if( 0 != glob )
   {
      // initialize the built-in JS objects and the global object
      JSBool builtins = JS_InitStandardClasses(cx, glob);
    
      if( builtins )
      {
         printf( "initialized builtins\n" );
         if( JS_DefineFunctions( cx, glob, shell_functions) )
         {
            if( JS_DefineFunctions( cx, glob, shell_functions2) )
            {
               initJSCurl( cx, glob );
               initJSImage( cx, glob );
               initJSText( cx, glob );
               initJSScreen( cx, glob );
               initJSURL( cx, glob );
               initJSMP3( cx, glob );
               startChildMonitor();

               printf( "initialized jsCurl and jsImage\n" );

               curlCache_t &cache = getCurlCache();

               for( int arg = 1 ; arg < argc ; arg++ )
               {
                  char const *url = argv[arg];
                  printf( "evaluating %s\n", url );
                  curlFile_t f( cache.get( url, false ) );
                  if( f.isOpen() )
                  {
                     pushURL( f.getEffectiveURL() );

                     printf( "opened url\n" );
                     
                     JSScript *script= JS_CompileScript( cx, glob, (char const *)f.getData(), f.getSize(), url, 1 );
                     if( script )
                     {
                        printf( "compiled script\n" );
                        jsval rval; 
                        JSBool exec = JS_ExecuteScript( cx, glob, script, &rval );
                        if( exec )
                        {
                           printf( "executed\n" );
                        }
                        else
                           printf( "exec error\n" );
                        JS_DestroyScript( cx, script );
                     }
                     else
                        printf( "Error compiling script\n" );
                     
                     popURL();
                  }
                  else
                     printf( "Error opening url %s\n", url );
               }

               stopChildMonitor();  // stop trapping SIGCHLD signal
            
            }
            else
               printf( "Error defining shell functions\n" );
         }
         else
            printf( "Error defining shell functions\n" );
      }
      else
         printf( "Error creating builtins\n" );
   }
   else
      fprintf( stderr, "Error allocating global\n" );

   JS_DestroyContext( cx );

   return 0;

}

