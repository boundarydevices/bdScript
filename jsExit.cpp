/*
 * Module jsExit.cpp
 *
 * This module defines the initJSExit() routine as 
 * declared and described in jsExit.h
 *
 *
 * Change History : 
 *
 * $Log: jsExit.cpp,v $
 * Revision 1.1  2003-07-06 01:22:23  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsExit.h"
#include "codeQueue.h"

bool volatile exitRequested_ = false ;
int volatile exitStatus_ = 0 ;

static JSBool
jsExit( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   exitRequested_ = true ;
   if( ( 1 <= argc ) && JSVAL_IS_INT( argv[0] ) )
   {
      exitStatus_ = JSVAL_TO_INT( argv[0] );
   }
   else
      exitStatus_ = 0 ;

   abortCodeQueue();
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec exit_functions[] = {
    {"exit",          jsExit,       1 },
    {0}
};

bool initJSExit( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, exit_functions);
}

