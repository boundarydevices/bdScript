/*
 * Module curlThread.cpp
 *
 * This module defines the multi-threaded curl
 * methods as declared in curlThread.h
 *
 *
 * Change History : 
 *
 * $Log: curlThread.cpp,v $
 * Revision 1.5  2002-11-15 14:39:24  ericn
 * -added dummy return value
 *
 * Revision 1.4  2002/11/11 04:29:06  ericn
 * -moved headers
 *
 * Revision 1.3  2002/11/03 17:55:44  ericn
 * -modified to support synchronous gets and posts
 *
 * Revision 1.2  2002/11/02 04:12:00  ericn
 * -modified to set isLoaded flag
 *
 * Revision 1.1  2002/10/31 02:13:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "curlThread.h"
#include "js/jsapi.h"
#include "mtQueue.h"
#include "codeQueue.h"
#include "ultoa.h"
#include "jsGlobals.h"

static mutex_t     syncMutex_ ;
static condition_t syncCondition_ ;

void jsCurlOnComplete( jsCurlRequest_t &req, curlFile_t const &f )
{
   jsval     handlerVal ;
   JSString *handlerCode ;

   if( JS_GetProperty( req.cx_, req.rhObj_, "onLoad", &handlerVal ) 
       && 
       JSVAL_IS_STRING( handlerVal )
       && 
       ( 0 != ( handlerCode = JSVAL_TO_STRING( handlerVal ) ) ) )
   {
      if( !queueSource( req.lhObj_, JS_GetStringBytes( handlerCode ), "curlRequest::onLoad" ) )
         JS_ReportError( req.cx_, "Error queueing onLoad\n" );
   }
}

void jsCurlOnError( jsCurlRequest_t &req, curlFile_t const &f )
{
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
}


struct curlItem_t {
   curlItem_t( jsCurlRequest_t const &rhs,
               curlRequest_t   const &request,
               bool                   useCache,
               bool                   async )
      : jsRequest_( rhs ),
        httpRequest_( request ),
        useCache_( useCache ),
        async_( async )
   {
   }
   curlItem_t( void )
      : jsRequest_(), httpRequest_(), useCache_( false ){}

   jsCurlRequest_t jsRequest_ ;
   curlRequest_t   httpRequest_ ;
   bool            useCache_ ;
   bool            async_ ;
};

typedef mtQueue_t<curlItem_t> curlQueue_t ;

#define NUM_READERS 1
static curlQueue_t curlQueue_ ;
static pthread_t   readers_[NUM_READERS];

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
         curlRequest_t httpReq( JS_GetStringBytes( propStr ) );
         propStr = 0 ; // might be GC'd anyway

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
                     std::string paramName( JS_GetStringBytes( sParamName ) );
                        
                     jsval     propVal ;
                     JSString *sPropStr ;
                     if( JS_GetProperty( request.cx_, paramArr, paramName.c_str(), &propVal ) 
                         &&
                         ( 0 != ( sPropStr = JS_ValueToString( request.cx_, propVal ) ) ) )
                     {
                        char const *paramVal = JS_GetStringBytes( sPropStr );

                        if( '@' != paramVal[0] )
                           httpReq.addVariable( paramName.c_str(), paramVal );
                        else
                           httpReq.addFile( paramName.c_str(), paramVal+1 );
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

         bool const pushed = curlQueue_.push( curlItem_t( request, httpReq, useCache, async ) );
         if( pushed )
         {
            if( !async )
            {
               mutexLock_t lock( syncMutex_ );
               bool complete = syncCondition_.wait( lock );
               return complete ;
            }
            else
               return true ;
         }
         else
         {
            JS_ReportError( request.cx_, "Error queueing curl request" );
            return false ;
         }
      }
      else
         return false ;
   }
   else
      return false ;
}

static void *readerThread( void *arg )
{
   while( 1 )
   {
      curlItem_t item ;
      if( curlQueue_.pull( item ) )
      {
         curlFile_t f( getCurlCache().post( item.httpRequest_, item.useCache_ ) );

         mutexLock_t lock( item.async_ ? execMutex_ : syncMutex_ );

         if( f.isOpen() && ( 200 == f.getHttpCode() ) )
         {
            item.jsRequest_.onComplete( item.jsRequest_, f );
            JS_DefineProperty( item.jsRequest_.cx_, 
                               item.jsRequest_.lhObj_, 
                               "isLoaded",
                               JSVAL_TRUE,
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
         }
         else
         {
            std::string sErrorMsg ;
            if( !f.isOpen() )
               sErrorMsg = "error loading file" ;
            else if( 200 != f.getHttpCode() )
            {
               sErrorMsg = "bad http response code: " ;
               sErrorMsg += DECIMAL_VALUE( f.getHttpCode() );
            }
         
            JSString *errorStr = JS_NewStringCopyN( item.jsRequest_.cx_, sErrorMsg.c_str(), sErrorMsg.size() );
            JS_DefineProperty( item.jsRequest_.cx_, 
                               item.jsRequest_.lhObj_, 
                               "loadErrorMsg",
                               STRING_TO_JSVAL( errorStr ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            item.jsRequest_.onError( item.jsRequest_, f );
         }

         if( !item.async_ )
            syncCondition_.signal();
      }
      else
         break;
   }

   return 0 ;
}

void startCurlThreads( void )
{
   for( int i = 0 ; i < NUM_READERS ; i++ )
   {
      int create = pthread_create( readers_+i, 0, readerThread, 0 );
      if( 0 == create )
      {
      }
      else
         fprintf( stderr, "Error %m creating curl-reader thread\n" );
   }
}

void stopCurlThreads( void )
{
   curlQueue_.abort();

   for( int i = 0 ; i < NUM_READERS ; i++ )
   {
//      printf( "waiting for thread %lu\n", readers_[i] );
      void *exitStat ;
      int result ;
      if( 0 == ( result = pthread_join( readers_[i], &exitStat ) ) )
      {
//         printf( "done with thread %lu\n", readers_[i] );
      }
      else
         fprintf( stderr, "Error %d(%m) waiting for curlReader thread\n", result );
   }

//   printf( "curl threads stopped\n" );
}


