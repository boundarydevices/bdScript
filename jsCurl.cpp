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
 * Revision 1.22  2003-12-06 22:07:12  ericn
 * -added support for temp file and offset
 *
 * Revision 1.21  2003/11/28 14:09:56  ericn
 * -lock lhObj_ via jsval
 *
 * Revision 1.20  2003/03/23 01:10:03  ericn
 * -added file upload from string variable
 *
 * Revision 1.19  2003/01/06 04:30:00  ericn
 * -modified to allow unlink() of filter before destruction
 *
 * Revision 1.18  2002/12/15 20:01:52  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.17  2002/12/03 13:36:13  ericn
 * -collapsed code and objects for curl transfers
 *
 * Revision 1.16  2002/12/02 15:19:22  ericn
 * -two-level callback with code queue filter
 *
 * Revision 1.15  2002/11/30 23:45:27  ericn
 * -removed debug msg
 *
 * Revision 1.14  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.13  2002/11/30 16:29:07  ericn
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
#include <unistd.h>

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
   std::string tempFile ;
   getCurlCache().getTempFileName( req.handle_, tempFile );
   sData = JS_NewStringCopyN( req.cx_, tempFile.c_str(), tempFile.size() );
   JS_DefineProperty( req.cx_, req.lhObj_, "tmpFile",
                      STRING_TO_JSVAL( sData ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   unsigned dataOffs ;
   getCurlCache().getDataOffset( req.handle_, dataOffs );
   if( ( 0 < dataOffs ) && ( dataOffs < size ) )
   {
      JS_DefineProperty( req.cx_, req.lhObj_, "tmpOffs",
                         INT_TO_JSVAL( dataOffs ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
   }
   else
      JS_ReportError( req.cx_, "invalid tmp file offset: %u" );
}

static JSBool curlFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsCurlClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         
         if( queueCurlRequest( thisObj, argv[0], cx, curlFileOnComplete ) ) 
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

static void cbOnComplete( void *cbParam )
{
   jsCurlRequest_t &req = *(jsCurlRequest_t *)cbParam ;
   req.status_     = req.completed_ ;
   req.isComplete_ = true ;

   if( 0 != req.onComplete_ )
      req.onComplete_( req, req.data_, req.length_ );

   JS_DefineProperty( req.cx_, 
                      req.lhObj_, 
                      "isLoaded",
                      JSVAL_TRUE,
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   jsval rhval ;
   if( JS_GetProperty( req.cx_, 
                       req.lhObj_, 
                       "initializer",
                       &rhval ) 
       &&
       JSVAL_IS_OBJECT( rhval ) )
   {
      JSObject *rhObj = JSVAL_TO_OBJECT( rhval );
      jsval handlerVal ;
      if( JS_GetProperty( req.cx_, rhObj, "onLoad", &handlerVal ) 
          && 
          JSVAL_IS_STRING( handlerVal ) )
      {
         executeCode( req.lhObj_, handlerVal, "curlRequest::onLoad" );
      }
   }

   getCurlCache().closeHandle( req.handle_ );

   delete &req ;
}

static void cbOnFailure( void *cbParam )
{
   jsCurlRequest_t &req = *(jsCurlRequest_t *)cbParam ;
   req.status_     = req.failed_ ;
   req.isComplete_ = true ;

   JSString *errorStr = JS_NewStringCopyN( req.cx_, req.errorMsg_.c_str(), req.errorMsg_.size() );
   JS_DefineProperty( req.cx_, 
                      req.lhObj_, 
                      "loadErrorMsg",
                      STRING_TO_JSVAL( errorStr ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   jsval rhval ;
   if( JS_GetProperty( req.cx_, 
                       req.lhObj_, 
                       "initializer",
                       &rhval ) 
       &&
       JSVAL_IS_OBJECT( rhval ) )
   {
      JSObject *rhObj = JSVAL_TO_OBJECT( rhval );
      jsval handlerVal ;
      if( JS_GetProperty( req.cx_, rhObj, "onLoadError", &handlerVal ) 
          && 
          JSVAL_IS_STRING( handlerVal ) )
      {
         executeCode( req.lhObj_, handlerVal, "curlRequest::onLoadError" );
      }
   }
   
   delete &req ;
}

static void cbOnCancel( void *cbParam )
{
   jsCurlRequest_t &req = *(jsCurlRequest_t *)cbParam ;
   req.status_     = req.cancelled_ ;
   req.isComplete_ = true ;

   jsval rhval ;
   if( JS_GetProperty( req.cx_, 
                       req.lhObj_, 
                       "initializer",
                       &rhval ) 
       &&
       JSVAL_IS_OBJECT( rhval ) )
   {
      JSObject *rhObj = JSVAL_TO_OBJECT( rhval );
      jsval handlerVal ;
      if( JS_GetProperty( req.cx_, rhObj, "onCancel", &handlerVal ) 
          && 
          JSVAL_IS_STRING( handlerVal ) )
      {
         executeCode( req.lhObj_, handlerVal, "curlRequest::onCancel" );
      }
   }

   delete &req ;
}

static void cbOnSize( void *cbParam )
{
   jsCurlRequest_t &req = *(jsCurlRequest_t *)cbParam ;
   JS_DefineProperty( req.cx_, 
                      req.lhObj_, 
                      "expectedSize",
                      INT_TO_JSVAL( req.expectedSize_ ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   jsval rhval ;
   if( JS_GetProperty( req.cx_, 
                       req.lhObj_, 
                       "initializer",
                       &rhval ) 
       &&
       JSVAL_IS_OBJECT( rhval ) )
   {
      JSObject *rhObj = JSVAL_TO_OBJECT( rhval );
      jsval handlerVal ;
      if( JS_GetProperty( req.cx_, rhObj, "onSize", &handlerVal ) 
          && 
          JSVAL_IS_STRING( handlerVal ) )
      {
         executeCode( req.lhObj_, handlerVal, "curlRequest::onSize" );
      }
   }
}

static void cbOnProgress( void *cbParam )
{
   jsCurlRequest_t &req = *(jsCurlRequest_t *)cbParam ;

   JS_DefineProperty( req.cx_, 
                      req.lhObj_, 
                      "readSoFar",
                      INT_TO_JSVAL( req.bytesSoFar_ ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   jsval rhval ;
   if( JS_GetProperty( req.cx_, 
                       req.lhObj_, 
                       "initializer",
                       &rhval ) 
       &&
       JSVAL_IS_OBJECT( rhval ) )
   {
      JSObject *rhObj = JSVAL_TO_OBJECT( rhval );
      jsval handlerVal ;
      if( JS_GetProperty( req.cx_, rhObj, "onProgress", &handlerVal ) 
          && 
          JSVAL_IS_STRING( handlerVal ) )
      {
         executeCode( req.lhObj_, handlerVal, "curlRequest::onProgress" );
      }
   }
}

static void jsCurlOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size, unsigned long handle )
{
   req.status_ = jsCurlRequest_t::completed_ ;
   req.data_   = data ;
   req.length_ = size ;
   req.handle_ = handle ;

   queueCallback( cbOnComplete, &req );
}

static void jsCurlOnFailure( jsCurlRequest_t &req, std::string const &errorMsg )
{
   req.errorMsg_ = errorMsg ;
   char volatile c = req.errorMsg_[0]; // forcibly detach from errorMsg
   queueCallback( cbOnFailure, &req );
}

static void jsCurlOnCancel( jsCurlRequest_t &req )
{
   queueCallback( cbOnFailure, &req );
}

static void jsCurlOnSize( jsCurlRequest_t &req, unsigned long size )
{
   req.expectedSize_ = size ;
   queueCallback( cbOnSize, &req );
}

static void jsCurlOnProgress( jsCurlRequest_t &req, unsigned long numReadSoFar )
{
   req.bytesSoFar_ = numReadSoFar ;
   queueCallback( cbOnProgress, &req );
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
     JSContext   *cx,
     bool         async,
     onComplete_t onComplete )
   : lhObj_( lhObj ),
     lhVal_( OBJECT_TO_JSVAL( lhObj ) ),
     cx_( cx ),
     async_( async ),
     isComplete_( false ),
     status_( inTransit_ ),
     data_( 0 ),
     length_( 0 ),
     handle_( 0 ),
     errorMsg_(),
     expectedSize_( 0 ),
     bytesSoFar_( 0 ),
     callingThread_( pthread_self() ),
     onComplete_( onComplete )
{
   JS_AddRoot( cx, &lhVal_ );
}

jsCurlRequest_t :: ~jsCurlRequest_t( void )
{
   JS_RemoveRoot( cx_, &lhVal_ );
   memset( this, 0, sizeof( this ) );
}

class curlCodeFilter_t : public codeFilter_t {
public:
   curlCodeFilter_t( jsCurlRequest_t &req )
      : req_( req ){}

   ~curlCodeFilter_t( void ){ unlink(); } // unlink before destructor 

   virtual bool isHandled( callback_t cb, void *cbParam )
   {
      if( &req_ == cbParam )
      {
         cb( cbParam );
         return true ;
      } // it's our request
      else
      {
         return false ;
      }
   }

   virtual bool isDone( void )
   {
      return req_.isComplete_ ;
   }

private:
   jsCurlRequest_t &req_ ;
};

bool queueCurlRequest
   ( JSObject                     *lhObj, 
     jsval                         initializer,
     JSContext                    *cx,
     jsCurlRequest_t::onComplete_t onComplete )
{
   bool const async = (cx->fp->flags & JSFRAME_CONSTRUCTING);
   
   JS_DefineProperty( cx, lhObj, "isLoaded",
                      JSVAL_FALSE,
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );

   JS_DefineProperty( cx, lhObj, "initializer",
                      initializer,
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   JSObject *const rhObj = JSVAL_TO_OBJECT( initializer );
      
   jsCurlRequest_t &request = *( new jsCurlRequest_t( lhObj, cx, async, onComplete ) );
   jsval urlVal ;
   JSString *propStr ;

   if( JS_GetProperty( request.cx_, rhObj, "url", &urlVal ) 
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
         if( JS_GetProperty( request.cx_, rhObj, "useCache", &useCacheVal ) 
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
         if( JS_GetProperty( request.cx_, rhObj, "urlParams", &paramArrVal ) 
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
                     if( JS_GetProperty( request.cx_, paramArr, paramName.c_str(), &propVal ) )
                     {
                        JSType const valType = JS_TypeOfValue( cx, propVal );
                        if( ( JSTYPE_STRING == valType )
                            &&
                            ( 0 != ( sPropStr = JS_ValueToString( request.cx_, propVal ) ) ) )
                        {
                           char const *paramVal = JS_GetStringBytes( sPropStr );
                           if( '_' == paramName[0] )
                           {
                              curl_formadd( &postHead, &paramTail, 
                                            CURLFORM_COPYNAME, paramName.c_str(),
                                            CURLFORM_BUFFER, paramName.c_str(), 
                                            CURLFORM_BUFFERPTR, paramVal, 
                                            CURLFORM_BUFFERLENGTH, JS_GetStringLength( sPropStr ), 
                                            CURLFORM_END ); 
                           } // binary file from string
                           else if( '@' == paramVal[0] )
                           {
                              curl_formadd( &postHead, &paramTail, 
                                            CURLFORM_COPYNAME, paramName.c_str(),
                                            CURLFORM_FILE, paramVal+1,
                                            CURLFORM_END );
                           } // string is file name
                           else
                           {
                              curl_formadd( &postHead, &paramTail, 
                                            CURLFORM_COPYNAME, paramName.c_str(),
                                            CURLFORM_COPYCONTENTS, paramVal,
                                            CURLFORM_END );
                           } // string is value
                        } // string value
                        else 
                        {
                           JS_ReportError( request.cx_, "Invalid urlParam value:must be string" );
                        }
                     }
                     else
                        JS_ReportError( request.cx_, "reading urlParam value" );
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

         if( request.async_ )
         {
            if( 0 == postHead )
               getCurlCache().get( absolute, &request, defaultCallbacks_ );
            else
               getCurlCache().post( absolute, postHead, &request, defaultCallbacks_ );
            return true ;
         } // asynchronous
         else
         {
            curlCodeFilter_t filter( request );

            if( 0 == postHead )
               getCurlCache().get( absolute, &request, defaultCallbacks_ );
            else
               getCurlCache().post( absolute, postHead, &request, defaultCallbacks_ );

            filter.wait();
            
            return true ;
         } // synchronous, install code filter
      }
   }

   delete &request ;

   return false ;
}
