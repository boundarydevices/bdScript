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
 * Revision 1.2  2002-09-28 17:04:16  ericn
 * -changed #include references
 *
 * Revision 1.1.1.1  2002/09/28 16:50:46  ericn
 * -Initial import
 *
 *
 * Copyright Ticketmaster Technologies, Inc. 2002
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* include the JS engine API header */
#include "js/jsstddef.h"
#include "js/jsapi.h"

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
UrlExec(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN i ;
    
    for (i = 0; i < argc; i++) {
        JSString *str = JS_ValueToString(cx, argv[i]);
        if( str )
        {
           char const *cURL = JS_GetStringBytes( str );
           urlFile_t f( cURL );
           if( f.isOpen() )
           {
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
    {"urlExec",         UrlExec,        0},
    {0}
};

/* main function sets up global JS variables, including run time,
 * a context, and a global object, then initializes the JS run time,
 * and creates a context. */

int main(int argc, char **argv)
{
   // initialize the JS run time, and return result in rt
   JSRuntime * const rt = JS_NewRuntime(8L * 1024L * 1024L);
 
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
            for( int arg = 1 ; arg < argc ; arg++ )
            {
               char const *url = argv[arg];
               printf( "evaluating %s\n", url );
               urlFile_t f( url );
               if( f.isOpen() )
               {
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
               }
               else
                  printf( "Error opening url %s\n", url );
            }
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

