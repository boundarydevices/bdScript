/*
 * Module jsShell.cpp
 *
 * This module defines the Javascript support
 * for shell operations.
 *
 *
 * Change History : 
 *
 * $Log: jsShell.cpp,v $
 * Revision 1.1  2002-11-17 22:25:07  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsShell.h"
#include "nspr/prproces.h"
#include <unistd.h>

static JSBool
jsShell( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   PRProcessAttr *pAttr = PR_NewProcessAttr();
   if( pAttr )
   {
      static char shName[] = { "/bin/sh" };
      static char *args[] = { { shName }, 0 };
      PRProcess *pProc = PR_CreateProcess( shName, args, __environ, pAttr );
      if( pProc )
      {
         PRInt32 exitStat ;
         PRStatus stat = PR_WaitProcess(pProc, &exitStat );
         JS_ReportError( cx, "shell:%ld:%ld\n", exitStat, stat );
      }
      else
         JS_ReportError( cx, "Error starting process\n" );

      PR_DestroyProcessAttr( pAttr );
   }
   else
      JS_ReportError( cx, "Error creating process attr\n" );
   
   
   
   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"shell",         jsShell,      0 },
    {0}
};

bool initJSShell( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}

