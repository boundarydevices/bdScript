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
 * Revision 1.11  2002-11-30 00:31:57  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.10  2002/11/05 15:15:39  ericn
 * -fixed to initialize members
 *
 * Revision 1.9  2002/11/03 17:55:51  ericn
 * -modified to support synchronous gets and posts
 *
 * Revision 1.8  2002/10/31 02:10:46  ericn
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
#include "relativeURL.h"
#include "jsGlobals.h"
#include "codeQueue.h"
#include "ccActiveURL.h"

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
  {"data",           CURLFILE_DATA,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static void curlFileOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   JSString *sData = JS_NewStringCopyN( req.cx_, (char const *)data, size );
   JS_DefineProperty( req.cx_, req.lhObj_, "data",
                      STRING_TO_JSVAL( sData ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   jsCurlOnComplete( req, data, size );
}

static JSBool curlFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
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
         request.onComplete_ = curlFileOnComplete ;
         request.onFailure_  = jsCurlOnFailure ;
         request.onCancel_   = jsCurlOnCancel ;
         request.onSize_     = jsCurlOnSize ; 
         request.onProgress_ = jsCurlOnProgress ;
         request.lhObj_      = thisObj ;
         request.rhObj_      = rhObj ;
         request.cx_         = cx ;

         if( queueCurlRequest( request, ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) ) ) )
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

bool initJSCurl( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsCurlClass_,
                       /* native constructor function and min arg count */
                       curlFile, 1,
                       curlFileProperties_, 0,
                       0, 0 );
   if( rval )
   {
      return true ;
   }
   
   return false ;
}

static semaphore_t syncSem_ ;

void jsCurlOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   jsval     handlerVal ;
   JSString *handlerCode ;

   JSString *const trueString = JS_NewStringCopyN( req.cx_, "true", 4 );
   JS_DefineProperty( req.cx_, 
                      req.lhObj_, 
                      "isLoaded",
                      STRING_TO_JSVAL( trueString ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   if( JS_GetProperty( req.cx_, req.rhObj_, "onLoad", &handlerVal ) 
       && 
       JSVAL_IS_STRING( handlerVal )
       && 
       ( 0 != ( handlerCode = JSVAL_TO_STRING( handlerVal ) ) ) )
   {
      if( !queueSource( req.lhObj_, JS_GetStringBytes( handlerCode ), "curlRequest::onLoad" ) )
         JS_ReportError( req.cx_, "Error queueing onLoad\n" );
   }
   
   syncSem_.signal();
}

void jsCurlOnFailure( jsCurlRequest_t &req, std::string const &errorMsg )
{
   JSString *errorStr = JS_NewStringCopyN( req.cx_, errorMsg.c_str(), errorMsg.size() );
   JS_DefineProperty( req.cx_, 
                      req.lhObj_, 
                      "loadErrorMsg",
                      STRING_TO_JSVAL( errorStr ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   jsval     handlerVal ;
   JSString *handlerCode ;

   if( JS_GetProperty( req.cx_, req.rhObj_, "onLoadError", &handlerVal ) 
       && 
       JSVAL_IS_STRING( handlerVal )
       && 
       ( 0 != ( handlerCode = JSVAL_TO_STRING( handlerVal ) ) ) )
   {
      if( !queueSource( req.lhObj_, JS_GetStringBytes( handlerCode ), "curlRequest::onLoadError" ) )
         JS_ReportError( req.cx_, "Error queueing onLoadError\n" );
   }
   
   syncSem_.signal();
}

void jsCurlOnCancel( jsCurlRequest_t &req )
{
   jsval     handlerVal ;
   JSString *handlerCode ;

   if( JS_GetProperty( req.cx_, req.rhObj_, "onCancel", &handlerVal ) 
       && 
       JSVAL_IS_STRING( handlerVal )
       && 
       ( 0 != ( handlerCode = JSVAL_TO_STRING( handlerVal ) ) ) )
   {
      if( !queueSource( req.lhObj_, JS_GetStringBytes( handlerCode ), "curlRequest::onCancel" ) )
         JS_ReportError( req.cx_, "Error queueing onCancel\n" );
   }
   
   syncSem_.signal();
}

void jsCurlOnSize( jsCurlRequest_t &req, unsigned long size )
{
   JS_DefineProperty( req.cx_, 
                      req.lhObj_, 
                      "expectedSize",
                      INT_TO_JSVAL( size ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   jsval     handlerVal ;
   JSString *handlerCode ;

   if( JS_GetProperty( req.cx_, req.rhObj_, "onSize", &handlerVal ) 
       && 
       JSVAL_IS_STRING( handlerVal )
       && 
       ( 0 != ( handlerCode = JSVAL_TO_STRING( handlerVal ) ) ) )
   {
      if( !queueSource( req.lhObj_, JS_GetStringBytes( handlerCode ), "curlRequest::onSize" ) )
         JS_ReportError( req.cx_, "Error queueing onSize\n" );
   }
   
   syncSem_.signal();
}

void jsCurlOnProgress( jsCurlRequest_t &req, unsigned long numReadSoFar )
{
   JS_DefineProperty( req.cx_, 
                      req.lhObj_, 
                      "readSoFar",
                      INT_TO_JSVAL( numReadSoFar ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   jsval     handlerVal ;
   JSString *handlerCode ;

   if( JS_GetProperty( req.cx_, req.rhObj_, "onProgress", &handlerVal ) 
       && 
       JSVAL_IS_STRING( handlerVal )
       && 
       ( 0 != ( handlerCode = JSVAL_TO_STRING( handlerVal ) ) ) )
   {
      if( !queueSource( req.lhObj_, JS_GetStringBytes( handlerCode ), "curlRequest::onProgress" ) )
         JS_ReportError( req.cx_, "Error queueing onProgress\n" );
   }
   
   syncSem_.signal();
}

static curlCallbacks_t const defaultCallbacks_ = {
   (curlCacheComplete_t)jsCurlOnComplete,
   (curlCacheFailure_t)jsCurlOnFailure,
   (curlCacheCancel_t)jsCurlOnCancel,
   (curlCacheSize_t)jsCurlOnSize,
   (curlCacheProgress_t)jsCurlOnProgress
};

bool queueCurlRequest( jsCurlRequest_t &request,
                       bool             async )
{
   jsval urlVal ;
   JSString *propStr ;

   if( JS_GetProperty( request.cx_, request.rhObj_, "url", &urlVal ) 
       &&
       JSVAL_IS_STRING( urlVal ) 
       &&
       ( 0 != ( propStr = JS_ValueToString( request.cx_, urlVal ) ) ) )
   {
      JSString *propStr = JS_ValueToString( request.cx_, urlVal );
      if( propStr )
      {
         curlCallbacks_t callbacks = defaultCallbacks_ ;

         callbacks.onComplete_ = (curlCacheComplete_t)request.onComplete_;
         callbacks.onFailure_  = (curlCacheFailure_t)request.onFailure_ ;
         callbacks.onCancel_   = (curlCacheCancel_t)request.onCancel_ ;
         callbacks.onSize_     = (curlCacheSize_t)request.onSize_ ;
         callbacks.onProgress_ = (curlCacheProgress_t)request.onProgress_ ;
         
         std::string const url( JS_GetStringBytes( propStr ), JS_GetStringLength( propStr ) );

         bool useCache = true ;
         jsval  useCacheVal ;
         JSBool jsUseCache ;
         if( JS_GetProperty( request.cx_, request.rhObj_, "useCache", &useCacheVal ) 
             &&
             JSVAL_IS_BOOLEAN( useCacheVal )
             && 
             JS_ValueToBoolean( request.cx_, useCacheVal, &jsUseCache ) )
         {
            useCache = jsUseCache ;
         } // have optional useCache

         struct HttpPost *postHead = 0 ;

         jsval     paramArrVal ;
         JSObject *paramArr ;
         if( JS_GetProperty( request.cx_, request.rhObj_, "urlParams", &paramArrVal ) 
             &&
             JSVAL_IS_OBJECT( paramArrVal ) 
             &&
             ( 0 != ( paramArr = JSVAL_TO_OBJECT( paramArrVal ) ) ) )
         {
            JSIdArray *ida = JS_Enumerate( request.cx_, paramArr );
            if( ida )
            {
               struct HttpPost* paramTail = NULL;
               
               for( unsigned i = 0 ; i < ida->length ; i++ )
               {
                  jsval     idVal ;
                  JSString *sParamName ;
                  if( JS_IdToValue( request.cx_, ida->vector[i], &idVal ) 
                      &&
                      JSVAL_IS_STRING( idVal )
                      &&
                      ( 0 != ( sParamName = JSVAL_TO_STRING( idVal ) ) ) )
                  {
                     std::string paramName( JS_GetStringBytes( sParamName ), JS_GetStringLength( sParamName ) );

                     jsval     propVal ;
                     JSString *sPropStr ;
                     if( JS_GetProperty( request.cx_, paramArr, paramName.c_str(), &propVal ) 
                         &&
                         ( 0 != ( sPropStr = JS_ValueToString( request.cx_, propVal ) ) ) )
                     {
                        char const *paramVal = JS_GetStringBytes( sPropStr );
                        if( '@' != paramVal[0] )
                        {
                           curl_formadd( &postHead, &paramTail, 
                                         CURLFORM_COPYNAME, paramName.c_str(),
                                         CURLFORM_COPYCONTENTS, paramVal,
                                         CURLFORM_END );
                        }
                        else
                        {
                           curl_formadd( &postHead, &paramTail, 
                                         CURLFORM_COPYNAME, paramName.c_str(),
                                         CURLFORM_FILE, paramVal+1,
                                         CURLFORM_END );
                        }
                     }
                     else
                        JS_ReportError( request.cx_, "Invalid urlParam value" );
                  }
                  else
                     JS_ReportError( request.cx_, "Invalid urlParam" );
               }
               JS_DestroyIdArray( request.cx_, ida );
            } // able to enumerate parameters
         } // have optional urlParams
         else
         {
         }

         if( 0 == postHead )
         {
            getCurlCache().get( url, &request, callbacks );
         }
         else
         {
            getCurlCache().post( url, postHead, &request, callbacks );
         }
         
         if( !async )
         {
            bool complete = syncSem_.wait();
            return complete ;
         }
         else
            return true ;
      }
      else
         return false ;
   }
   else
      return false ;
}


