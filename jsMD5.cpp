/*
 * Module jsMD5.cpp
 *
 * This module defines the various md5 hash
 * routines as declared in jsMD5.h
 *
 *
 * Change History : 
 *
 * $Log: jsMD5.cpp,v $
 * Revision 1.1  2004-09-27 04:51:31  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include "jsMD5.h"
#include "md5.h"

static jsval md5ToRval( JSContext *cx, md5_t const &md5 )
{
   unsigned const nalloc = sizeof( md5.md5bytes_)*2+1 ;
   char * const stringMem = (char *)JS_malloc( cx, nalloc );
   char *nextOut = stringMem ;
   for( unsigned i = 0 ; i < sizeof( md5.md5bytes_); i++ )
      nextOut += sprintf( nextOut, "%02x", md5.md5bytes_[i] );
   *nextOut = '\0' ;

   return STRING_TO_JSVAL( JS_NewString( cx, stringMem, nalloc - 1 ) );
}

static JSBool
jsMD5( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sArg = JSVAL_TO_STRING( argv[0] );
      md5_t md5 ;
      getMD5( JS_GetStringBytes( sArg ), JS_GetStringLength( sArg ), md5 );
      *rval = md5ToRval( cx, md5 );
   }
   else
      JS_ReportError( cx, "Usage: md5( string );" );

   return JS_TRUE;
}

static JSBool
jsMD5File( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sArg = JSVAL_TO_STRING( argv[0] );
      md5_t md5 ;
      if( getMD5( JS_GetStringBytes( sArg ), md5 ) )
         *rval = md5ToRval( cx, md5 );
   }
   else
      JS_ReportError( cx, "Usage: md5( string );" );

   return JS_TRUE;
}

static JSBool
jsMD5Cram( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sArg = JSVAL_TO_STRING( argv[0] );
      md5_t md5 ;
      if( md5Cramfs( JS_GetStringBytes( sArg ), md5 ) )
         *rval = md5ToRval( cx, md5 );
   }
   else
      JS_ReportError( cx, "Usage: md5( string );" );

   return JS_TRUE;
}

static JSFunctionSpec md5_functions[] = {
    {"md5",             jsMD5,            0},
    {"md5File",         jsMD5File,        0},
    {"md5Cram",         jsMD5Cram,        0},
    {0}
};


bool initJSMD5( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, md5_functions);
}


