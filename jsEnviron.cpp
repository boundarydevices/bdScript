/*
 * Module jsEnviron.cpp
 *
 * This module defines the Javascript wrappers for
 * the getenv() and setenv() calls.
 *
 *
 * Change History : 
 *
 * $Log: jsEnviron.cpp,v $
 * Revision 1.1  2002-12-12 15:41:34  ericn
 * -added environment routines
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "jsEnviron.h"
#include <stdlib.h>

static JSBool
jsGetEnv( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_VOID ;

   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sVarName = JSVAL_TO_STRING( argv[0] );
      char const * const value = getenv( JS_GetStringBytes( sVarName ) );
      if( value )
         *rval = STRING_TO_JSVAL( JS_NewStringCopyZ( cx, value ) );
   }
   else
      JS_ReportError( cx, "Usage : getenv( 'varname' );" );

   return JS_TRUE ;
}

static JSBool
jsSetEnv( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_VOID ;

   if( ( 2 == argc ) && JSVAL_IS_STRING( argv[0] ) && JSVAL_IS_STRING( argv[1] ) )
   {
      JSString *sVarName = JSVAL_TO_STRING( argv[0] );
      JSString *sValue   = JSVAL_TO_STRING( argv[1] );
      setenv( JS_GetStringBytes( sVarName ), JS_GetStringBytes( sValue ), 1 );
   }
   else
      JS_ReportError( cx, "Usage : setenv( 'varname', 'value' );" );

   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"getenv",         jsGetEnv,      0 },
    {"setenv",         jsSetEnv,      0 },
    {0}
};

bool initJSEnv( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}

