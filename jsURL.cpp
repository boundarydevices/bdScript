/*
 * Module jsURL.cpp
 *
 * This module defines the initialization routine
 * for Javascript URL routines and the C++ versions
 * of them as declared in jsURL.h
 *
 *
 * Change History : 
 *
 * $Log: jsURL.cpp,v $
 * Revision 1.3  2002-10-25 02:20:46  ericn
 * -moved non-Javascript stuff to relativeURL.cpp
 *
 * Revision 1.2  2002/10/24 13:16:01  ericn
 * -added pushURL() and popURL()
 *
 * Revision 1.1  2002/10/20 16:31:23  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsURL.h"
#include "relativeURL.h"

static JSBool
jsCurrentURL( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   JSString *jsCurrent = 0 ;

   std::string current ;
   if( currentURL( current ) )
   {
      jsCurrent = JS_NewStringCopyN( cx, current.c_str(), current.size() );
   }
   else
      jsCurrent = JS_NewStringCopyZ( cx, "unknown" );

   *rval = STRING_TO_JSVAL( jsCurrent );
   return JS_TRUE ;
}

static JSBool
jsAbsoluteURL( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) && ( JSVAL_IS_STRING( argv[0] ) ) )
   {
      JSString *urlString = JS_ValueToString( cx, argv[0] );
      std::string url( JS_GetStringBytes( urlString ), JS_GetStringLength( urlString ) );
      std::string absolute ;
      if( absoluteURL( url, absolute ) )
         *rval = STRING_TO_JSVAL( JS_NewStringCopyN( cx, absolute.c_str(), absolute.size() ) );
      else
         *rval = JSVAL_FALSE ;
   }
   else
      *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static JSBool
jsIsRelativeURL( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) && ( JSVAL_IS_STRING( argv[0] ) ) )
   {
      JSString *urlString = JS_ValueToString( cx, argv[0] );
      std::string url( JS_GetStringBytes( urlString ), JS_GetStringLength( urlString ) );
      if( isRelativeURL( url ) )
         *rval = JSVAL_TRUE ;
      else
         *rval = JSVAL_FALSE ;
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static JSFunctionSpec image_functions[] = {
    {"currentURL",           jsCurrentURL,       1 },
    {"absoluteURL",          jsAbsoluteURL,      1 },
    {"isRelativeURL",        jsIsRelativeURL,    1 },
    {0}
};

bool initJSURL( JSContext *cx, JSObject *glob )
{
   if( JS_DefineFunctions( cx, glob, image_functions) )
   {
      return true ;
   }
   else
      return false ;
}

