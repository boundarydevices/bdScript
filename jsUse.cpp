/*
 * Module jsUse.cpp
 *
 * This module defines the internals of the use() directive,
 * which allows re-use of Javascript modules.
 *
 *
 * Change History : 
 *
 * $Log: jsUse.cpp,v $
 * Revision 1.2  2004-01-02 23:36:16  ericn
 * -remove debug stmt
 *
 * Revision 1.1  2003/01/20 06:24:01  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsUse.h"
#include "urlFile.h"
#include "relativeURL.h"
#include "jsCurl.h"

enum jsImage_tinyId {
   LIBRARY_ISLOADED,
   LIBRARY_SOURCE, 
};


JSClass jsLibraryClass_ = {
  "library",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

JSClass jsUseClass_ = {
  "use",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec libraryProperties_[] = {
  {"isLoaded",      LIBRARY_ISLOADED,  JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"source",        LIBRARY_SOURCE,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

JSBool
jsLibraryEvaluate( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec libraryMethods_[] = {
    {"eval",         jsLibraryEvaluate,           0 },
    {0}
};


static void libraryOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   JSString *sSource = JS_NewStringCopyN( req.cx_, (char const *)data, size );
   JS_DefineProperty( req.cx_, req.lhObj_, "source",
                      STRING_TO_JSVAL( sSource ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   jsval rv ;
   if( !JS_EvaluateScript( req.cx_, req.lhObj_, (char const *)data, size, "<library>", 0, &rv ) )
      JS_ReportError( req.cx_, "evaluating library" );
}

static JSBool
use(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval* rval)
{
   JSString *sLib ;
   if( ( 1 == argc )
       && 
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject * const thisObj = JS_NewObject( cx, &jsUseClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         if( !queueCurlRequest( thisObj, argv[0], cx, libraryOnComplete ) )
            JS_ReportError( cx, "Error queueing curlRequest" );
      }
      else
      {
         JS_ReportError( cx, "producing URL" );
         *rval = JSVAL_FALSE ;
      }
   }
   else
   {
      JS_ReportError( cx, "Usage: use( { url:'url' } );" );
      *rval = JSVAL_FALSE ;
   }

   return JS_TRUE ;
}

bool initJSUse( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsUseClass_,
                                  use, 1,
                                  libraryProperties_, 
                                  libraryMethods_,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      return false ;
}

