/*
 * Module jsCurl.cpp
 *
 * This module defines the curl classes and routines, and
 * the initialization routine as declared and described in
 * jsCurl.h
 *
 *
 * Change History : 
 *
 * $Log: jsCurl.cpp,v $
 * Revision 1.8  2002-10-31 02:10:46  ericn
 * -modified curlFile() constructor to be multi-threaded, use rh object
 *
 * Revision 1.7  2002/10/25 04:49:05  ericn
 * -removed debug statements
 *
 * Revision 1.6  2002/10/15 05:00:39  ericn
 * -added error messages
 *
 * Revision 1.5  2002/10/13 15:52:28  ericn
 * -made return value an object
 *
 * Revision 1.4  2002/10/13 14:36:13  ericn
 * -removed curlPost(), fleshed out variable handling
 *
 * Revision 1.3  2002/10/13 13:50:57  ericn
 * -merged curlGet() and curlPost() with curlFile object
 *
 * Revision 1.2  2002/10/06 14:54:10  ericn
 * -added Finalize, removed debug statements
 *
 * Revision 1.1  2002/09/29 17:36:23  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsCurl.h"
#include "js/jsstddef.h"
#include "js/jscntxt.h"
#include "js/jsapi.h"
#include "js/jslock.h"
#include "urlFile.h"
#include <stdio.h>
#include "curlCache.h"
#include "relativeURL.h"
#include "curlThread.h"
#include "jsGlobals.h"

enum jsCurl_tinyid {
   CURLFILE_ISLOADED, 
   CURLFILE_WORKED, 
   CURLFILE_DATA, 
   CURLFILE_URL, 
   CURLFILE_HTTPCODE, 
   CURLFILE_FILETIME, 
   CURLFILE_MIMETYPE,
   CURLFILE_PARAMS
};

extern JSClass jsCurlClass_ ;

JSClass jsCurlClass_ = {
  "curlFile",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec curlFileProperties_[] = {
  {"isLoaded",       CURLFILE_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"worked",         CURLFILE_WORKED,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"data",           CURLFILE_DATA,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"url",            CURLFILE_URL,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"httpCode",       CURLFILE_HTTPCODE,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"fileTime",       CURLFILE_FILETIME,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"mimeType",       CURLFILE_MIMETYPE,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"params",         CURLFILE_PARAMS,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static JSBool curlFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       && 
       ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = js_NewObject( cx, &jsCurlClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         
         JS_DefineProperty( cx, thisObj, "isLoaded",
                            JSVAL_FALSE,
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JS_DefineProperty( cx, thisObj, "initializer",
                            argv[0],
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JSObject *const rhObj = JSVAL_TO_OBJECT( argv[0] );
         
         jsCurlRequest_t request ;
         request.onComplete = jsCurlOnComplete ;
         request.onError    = jsCurlOnError ;
         request.lhObj_ = thisObj ;
         request.rhObj_ = rhObj ;
         request.cx_    = cx ;
         request.mutex_ = &execMutex_ ;

         if( queueCurlRequest( request ) )
         {
            return JS_TRUE ;
         }
         else
         {
            JS_ReportError( cx, "Error queueing curlRequest" );
         }
      }
      else
         JS_ReportError( cx, "Error allocating curlFile" );
   }
   else
      JS_ReportError( cx, "Usage: new curlFile( {url:\"something\"} );" );
      
   return JS_FALSE ;

}

static JSBool returnFile( JSContext *cx, jsval *rval, curlFile_t &f )
{
   JSObject *obj = js_NewObject( cx, &jsCurlClass_, NULL, NULL );

   if( obj )
   {
      if( JS_DefineProperty( cx, obj, "worked", 
                             BOOLEAN_TO_JSVAL( f.isOpen() ), 
                             0, 0, 
                             JSPROP_ENUMERATE
                             |JSPROP_PERMANENT
                             |JSPROP_READONLY ) )
      {
         if( f.isOpen() )
         {
//            printf( "file opened\n" );
            JS_DefineProperty( cx, obj, "data",
                               STRING_TO_JSVAL( JS_NewStringCopyN( cx, (char const *)f.getData(), f.getSize() ) ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, obj, "url",
                               STRING_TO_JSVAL( JS_NewStringCopyZ( cx, (char const *)f.getEffectiveURL() ) ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, obj, "httpCode",
                               INT_TO_JSVAL( f.getHttpCode() ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, obj, "fileTime",
                               INT_TO_JSVAL( f.getFileTime() ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, obj, "mimeType",
                               STRING_TO_JSVAL( JS_NewStringCopyZ( cx, (char const *)f.getMimeType() ) ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );

            *rval = OBJECT_TO_JSVAL( obj );
         }
         else
         {
            JS_ReportError( cx, "error opening file\n" );
            *rval = JSVAL_FALSE ;
         }
      
         return JS_TRUE ;
      }
      else
         JS_ReportError( cx, "Error defining property\n" );
   }
   else
      JS_ReportError( cx, "Error allocating object\n" );
   
   return JS_FALSE ;
}

static JSBool
curlGet(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   //
   // need at least url. 
   //
   // If more parameters, cacheFlag(bool) is next ( must be bool )
   // and the remainder must be name/value pairs
   //
   // names beginning with '@' indicate files to upload
   // 
   //
   if( ( 1 <= argc )
       &&
       JSVAL_IS_STRING( argv[0] ) 
       &&
       ( ( 1 == argc ) 
         || 
         JSVAL_IS_BOOLEAN( argv[1] ) ) )      // second parameter must be boolean
   {
      JSString *str = JS_ValueToString( cx, argv[0] );
      if( str )
      {
         curlCache_t &cache = getCurlCache();
         char const *cURL = JS_GetStringBytes( str );
         bool const useCache = ( 1 == argc ) 
                               || 
                               ( 0 != JSVAL_TO_BOOLEAN( argv[1] ) );
         if( 2 < argc )
         {
            bool failed = false ;
            curlRequest_t request( cURL );
            for( uintN arg = 2 ; ( arg < argc-1 ) && !failed ; )
            {
               JSString *jsName = JS_ValueToString( cx, argv[arg++] );
               JSString *jsVal = JS_ValueToString( cx, argv[arg++] );
               if( ( 0 != jsName ) && ( 0 != jsVal ) )
               {
                  char const *cName = JS_GetStringBytes( jsName );
                  char const *cValue = JS_GetStringBytes( jsVal );
                  if( '@' != cName[0] )
                     request.addVariable( cName, cValue );
                  else
                     request.addFile( cName+1, cValue );
               }
               else
               {
                  failed = true ;
               }
            } // add each post variable

            if( !failed )
            {
               curlFile_t f( cache.post( request, useCache ) );
               return returnFile( cx, rval, f );
            }
         } // have parameters to post
         else
         {
            curlFile_t f( cache.get( cURL, useCache ) );
            return returnFile( cx, rval, f );
         } // simple get
      } // retrieved string

      *rval = JSVAL_FALSE ;
      return JS_TRUE ;

   } // need at least two params

   return JS_FALSE ;
}

static JSFunctionSpec curl_functions[] = {
    {"curlGet",         curlGet,        0},
    {0}
};

bool initJSCurl( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsCurlClass_,
                       /* native constructor function and min arg count */
                       curlFile, 1,
                       curlFileProperties_, 0,
                       0, 0 );
   if( rval )
   {
      return JS_DefineFunctions( cx, glob, curl_functions);
   }
   
   return false ;
}


