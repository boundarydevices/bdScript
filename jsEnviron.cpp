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
 * Revision 1.3  2004-09-25 14:19:39  ericn
 * -made return value from environ an object
 *
 * Revision 1.2  2004/03/17 14:17:36  ericn
 * -added environ() call
 *
 * Revision 1.1  2002/12/12 15:41:34  ericn
 * -added environment routines
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "jsEnviron.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "jsGlobals.h"

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

static JSBool
jsEnviron( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_VOID ;

   if( 0 == argc )
   {
      JSObject *arr = JS_NewObject( cx, &global_class, 0, obj );
      if( arr )
      {
         *rval = OBJECT_TO_JSVAL( arr );
         unsigned i ;
         for( i = 0 ; __environ[i]; i++ )
         {
            char *envItem = __environ[i];
            char *value = strchr( envItem, '=' );
            unsigned len = value-envItem ;
            char ntItem[256];
            memcpy( ntItem, envItem, len );
            ntItem[len] = 0 ;

            JSString *sValue = 0 ;
            if( value )
               sValue = JS_NewStringCopyZ( cx, value+1 );
            
            jsval jsv = (sValue ? STRING_TO_JSVAL( sValue ) : JSVAL_FALSE );
            if( !JS_DefineProperty( cx, arr, ntItem, jsv, 0, 0, JSPROP_ENUMERATE | JSPROP_READONLY ) )
               JS_ReportError( cx, "set env[%s]\n", ntItem );
         }
      }
      else
         JS_ReportError( cx, "Allocating environ array" );
   }
   else
      JS_ReportError( cx, "Usage : setenv( 'varname', 'value' );" );

   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"getenv",         jsGetEnv,      0 },
    {"setenv",         jsSetEnv,      0 },
    {"environ",        jsEnviron,      0 },
    {0}
};

bool initJSEnv( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}

