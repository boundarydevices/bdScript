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
 * Revision 1.7  2002-10-25 04:49:05  ericn
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

enum jsCurl_tinyid {
   CURLFILE_WORKED, 
   CURLFILE_DATA, 
   CURLFILE_URL, 
   CURLFILE_HTTPCODE, 
   CURLFILE_FILETIME, 
   CURLFILE_MIMETYPE
};

extern JSClass jsCurlClass_ ;

static void
jsCurl_finalize(JSContext *cx, JSObject *obj)
{
   urlFile_t *fURL = (urlFile_t *)JS_GetInstancePrivate( cx, obj, &jsCurlClass_, NULL);

	if( fURL )
   {
      printf( "finalizing object %p/%p\n", obj, fURL );
      delete fURL ;
      JS_SetPrivate( cx, obj, 0 );
	}
}

JSClass jsCurlClass_ = {
  "curlFile",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsCurl_finalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec curlFileProperties_[] = {
  {"worked",         CURLFILE_WORKED,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"data",           CURLFILE_DATA,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"url",            CURLFILE_URL,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"httpCode",       CURLFILE_HTTPCODE,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"fileTime",       CURLFILE_FILETIME,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"mimeType",       CURLFILE_MIMETYPE,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static JSBool curlFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( (argc == 1) && ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) ) )
   {
      if( JSVAL_IS_STRING( argv[0] ) )
      {
         obj = js_NewObject( cx, &jsCurlClass_, NULL, NULL );

         if( obj )
         {
            char const *url = JS_GetStringBytes( js_ValueToString( cx, argv[0]) );

            urlFile_t *fURL = new urlFile_t( (char const *)url );
            if( fURL )
            {
// printf( "new file %s/%p/%p\n", url, obj, fURL );
               if( JS_SetPrivate( cx, obj, fURL ) )
               {
                  *rval = OBJECT_TO_JSVAL(obj);
                  return JS_TRUE;
               }
               else
                  delete fURL ;
            }
         }
      }
   }
      
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
            printf( "error opening file\n" );
            *rval = JSVAL_FALSE ;
         }
      
         return JS_TRUE ;
      }
      else
         printf( "Error defining property\n" );
   }
   else
      printf( "Error allocating object\n" );
   
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
         JSVAL_IS_BOOLEAN( argv[1] ) ) )   // second parameter must be boolean
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


