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
 * Revision 1.13  2002-11-30 16:29:07  ericn
 * -fixed locking and allocation of requests
 *
 * Revision 1.12  2002/11/30 05:27:13  ericn
 * -modified to call custom handlers from defaults, fixed locking
 *
 * Revision 1.11  2002/11/30 00:31:57  ericn
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

static mutex_t     syncMutex_ ;
static condition_t syncSem_ ;

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
         
         if( queueCurlRequest( thisObj, rhObj, cx, (cx->fp->flags & JSFRAME_CONSTRUCTING), curlFileOnComplete ) ) 
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

static void doLock( jsCurlRequest_t &req )
{
   if( req.callingThread_ != pthread_self() )
   {
      int result = pthread_mutex_lock( req.async_ ? &execMutex_.handle_ : &syncMutex_.handle_ );
      if( 0 != result )
         fprintf( stderr, "interrupted!\n" );
   }
}

static void doUnlock( jsCurlRequest_t &req )
{
   if( req.callingThread_ != pthread_self() )
      pthread_mutex_unlock( req.async_ ? &execMutex_.handle_ : &syncMutex_.handle_ );
}

void jsCurlOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   doLock( req );
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
   
   if( 0 != req.onComplete_ )
      req.onComplete_( req, data, size );

   req.isComplete_ = true ;
   if( !req.async_ ) 
      syncSem_.signal();
   else
      delete &req ;

   doUnlock( req );
}

void jsCurlOnFailure( jsCurlRequest_t &req, std::string const &errorMsg )
{
   doLock( req );
    
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
   
   if( 0 != req.onFailure_ )
      req.onFailure_( req, errorMsg );

   req.isComplete_ = true ;
   if( !req.async_ ) 
      syncSem_.signal();
   else
      delete &req ;
   doUnlock( req );
}

void jsCurlOnCancel( jsCurlRequest_t &req )
{
   doLock( req );

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
   
   if( 0 != req.onCancel_  )
      req.onCancel_( req );

   req.isComplete_ = true ;
   if( !req.async_ ) 
      syncSem_.signal();
   else
      delete &req ;
   doUnlock( req );
}

void jsCurlOnSize( jsCurlRequest_t &req, unsigned long size )
{
   doLock( req );

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
   
   if( 0 != req.onSize_ )
      req.onSize_( req, size );
   
   doUnlock( req );
}

#include <unistd.h>
void jsCurlOnProgress( jsCurlRequest_t &req, unsigned long numReadSoFar )
{
   doLock( req );
//   mutexLock_t lock( req.async_ ? execMutex_ : syncMutex_ );

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

   if( 0 != req.onProgress_ )
      req.onProgress_( req, numReadSoFar );
   
   doUnlock( req );
}

static curlCallbacks_t const defaultCallbacks_ = {
   (curlCacheComplete_t)jsCurlOnComplete,
   (curlCacheFailure_t)jsCurlOnFailure,
   (curlCacheCancel_t)jsCurlOnCancel,
   (curlCacheSize_t)jsCurlOnSize,
   (curlCacheProgress_t)jsCurlOnProgress
};


jsCurlRequest_t :: jsCurlRequest_t
   ( JSObject    *lhObj, 
     JSObject    *rhObj, 
     JSContext   *cx,
     bool         async,
     onComplete_t onComplete,
     onFailure_t  onFailure,
     onCancel_t   onCancel,
     onSize_t     onSize,
     onProgress_t onProgress )
   : lhObj_( lhObj ),
     rhObj_( rhObj ),
     cx_( cx ),
     async_( async ),
     isComplete_( false ),
     callingThread_( pthread_self() ),
     onComplete_( onComplete ),
     onFailure_( onFailure ),
     onCancel_( onCancel ),
     onSize_( onSize ),
     onProgress_( onProgress )
{
}

jsCurlRequest_t :: ~jsCurlRequest_t( void )
{
   memset( this, 0, sizeof( this ) );
}

bool queueCurlRequest
   ( JSObject                     *lhObj, 
     JSObject                     *rhObj, 
     JSContext                    *cx,
     bool                          async,
     jsCurlRequest_t::onComplete_t onComplete,
     jsCurlRequest_t::onFailure_t  onFailure,
     jsCurlRequest_t::onCancel_t   onCancel,
     jsCurlRequest_t::onSize_t     onSize,
     jsCurlRequest_t::onProgress_t onProgress)
{
   jsCurlRequest_t &request = *( new jsCurlRequest_t( lhObj, rhObj, cx, async, 
                                                      onComplete, onFailure, onCancel, 
                                                      onSize, onProgress ) );
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
         std::string const url( JS_GetStringBytes( propStr ), JS_GetStringLength( propStr ) );
         std::string absolute ;
         absoluteURL( url, absolute );

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

         request.isComplete_ = false ;

         if( 0 == postHead )
         {
            getCurlCache().get( absolute, &request, defaultCallbacks_ );
         }
         else
         {
            getCurlCache().post( absolute, postHead, &request, defaultCallbacks_ );
         }

         if( !request.async_ )
         {
            mutexLock_t lock( syncMutex_ );
            bool const complete = request.isComplete_ || syncSem_.wait(lock);
            if( complete )
            {   
               delete &request ;
               return true ;
            }
         }
         else
         {
            return true ;
         }
      }
   }

   delete &request ;

}
