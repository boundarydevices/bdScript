/*
 * Module jsFlashVar.cpp
 *
 * This module defines the initialization routines
 * and functions for the flashVar global as described
 * in jsFlashVar.h
 *
 *
 * Change History : 
 *
 * $Log: jsFlashVar.cpp,v $
 * Revision 1.2  2004-02-07 13:40:53  ericn
 * -removed debug prints
 *
 * Revision 1.1  2004/02/07 12:14:51  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "jsFlashVar.h"
#include "flashVar.h"
#include <stdlib.h>
#include <string.h>

static JSObject *flashVarProto = NULL ;

static JSBool
jsGetFlashVar( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *str = JS_ValueToString(cx, argv[0]);
      if( str )
      {
         char const *value = readFlashVar( JS_GetStringBytes( str ) );
         JSString *sValue ;
         if( value )
         {
            sValue = JS_NewStringCopyZ( cx, value );
            free( (void *)value );
         }
         else
         {
            sValue = JS_NewStringCopyZ( cx, "" );
         }
         *rval = STRING_TO_JSVAL( sValue );
      }
      else
         JS_ReportError( cx, "resolving flashVar.get() parameter" );
   }
   else
      JS_ReportError( cx, "Usage: flashVar.get( varName );" );
   
   return JS_TRUE ;
}

static JSBool
jsSetFlashVar( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sVarName = JSVAL_TO_STRING( argv[0] );
      JSString *sValue = JS_ValueToString( cx, argv[1] );
      if( sVarName && sValue )
      {
         char const *varName = JS_GetStringBytes( sVarName );
         char const *value = JS_GetStringBytes( sValue );
         writeFlashVar( JS_GetStringBytes( sVarName ),
                        JS_GetStringBytes( sValue ) );
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "converting flashVar.set() parameter" );
   }
   else
      JS_ReportError( cx, "Usage: flashVar.set( varName, value );" );
   
   return JS_TRUE ;
}

static JSFunctionSpec flashVarMethods_[] = {
    {"get",         jsGetFlashVar,           0 },
    {"set",         jsSetFlashVar,           0 },
    {0}
};

JSClass jsFlashVarClass_ = {
  "flashVariable",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec flashVarProperties_[] = {
  {0,0,0}
};

static JSBool jsFlashVar( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   obj = JS_NewObject( cx, &jsFlashVarClass_, NULL, NULL );

   if( obj )
   {
      *rval = OBJECT_TO_JSVAL(obj);
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

bool initJSFlashVar( JSContext *cx, JSObject *glob )
{
   flashVarProto = JS_InitClass( cx, glob, NULL, &jsFlashVarClass_,
                                 jsFlashVar, 1,
                                 flashVarProperties_, 
                                 flashVarMethods_,
                                 0, 0 );
   if( flashVarProto )
   {
      JS_AddRoot( cx, &flashVarProto );
      JS_DefineProperty( cx, glob, "flashVar", 
                         OBJECT_TO_JSVAL( flashVarProto ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
   }
   else
      JS_ReportError( cx, "initializing flashVar class" );

   return false ;
}

