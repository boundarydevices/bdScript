/*
 * Module ccWorker.cpp
 *
 * This module defines the curl cache worker threads
 * and request queue.
 *
 *
 * Change History : 
 *
 * $Log: ccWorker.cpp,v $
 * Revision 1.1  2002-11-27 18:35:41  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "ccWorker.h"
#include <stdio.h>
#include <math.h>

static curlQueue_t *requestQueue_ = 0 ;

curlQueue_t &getCurlRequestQueue( void )
{
   if( 0 == requestQueue_ )
   {
      CURLcode result = curl_global_init( CURL_GLOBAL_ALL );
      if( CURLE_OK != result )
      {
         fprintf( stderr, "Error %d initializing curl\n", result );
         exit( 1 );
      }
      
      requestQueue_ = new curlQueue_t ;
   }
   return *requestQueue_ ;
}

static onCurlComplete_t     onComplete_ = 0 ;
static onCurlFailure_t      onFailure_ = 0 ;
static onCurlCancel_t       onCancel_ = 0 ;
static onCurlSize_t         onSize_ = 0 ;
static onCurlProgress_t     onProgress_ = 0 ;

struct progressData_t {
   curlRequest_t *request_ ;
   std::string   *data_ ;
   unsigned long  expectedBytes_ ;
};

static size_t writeData( void *buffer, size_t size, size_t nmemb, void *userp )
{
   unsigned const total = size*nmemb;
   
   progressData_t *pd = (progressData_t *)userp ;
   if( !*(pd->request_->cancel_) )
   {
      pd->data_->append( (char *)buffer, total );
      return total ;
   }
   else
      return 0 ;
}

static int progress_callback
   ( void *clientp,
     double dltotal,
     double dlnow,
     double ultotal,
     double ulnow)
{
   progressData_t *pd = (progressData_t *)clientp ;

   if( 0 == pd->expectedBytes_ )
   {
      if( 0.0 != dltotal )
      {
         unsigned long const expectedSize = (unsigned long)floor( dltotal );
         onSize_( *(pd->request_), expectedSize );
         pd->expectedBytes_ = expectedSize ;
      }
   }
   
   onProgress_( *(pd->request_), (unsigned long)floor( dlnow ) );
   
   return 0 ;
}

static void *readerThread( void *arg )
{
   curlQueue_t &queue = getCurlRequestQueue();

   while( 1 )
   {
      curlRequest_t request ;
      if( queue.pull( request ) )
      {
         std::string data ;
         std::string errorMsg ;
         CURL *cHandle = curl_easy_init();
         if( 0 != cHandle )
         {

            char errorBuf[CURL_ERROR_SIZE];
            errorBuf[0] = '\0' ;

            CURLcode result = curl_easy_setopt( cHandle, CURLOPT_ERRORBUFFER, errorBuf );

            if( 0 == result )
            {
               result = curl_easy_setopt( cHandle, CURLOPT_URL, request.url_.c_str() );
   
               if( 0 == result )
               {
                  result = curl_easy_setopt( cHandle, CURLOPT_WRITEFUNCTION, writeData );
                  if( 0 == result )
                  {
                     progressData_t pd ;
                     pd.request_ = &request ;
                     pd.data_    = &data ;

                     result = curl_easy_setopt( cHandle, CURLOPT_WRITEDATA, &pd );
                     if( 0 == result )
                     {
                        result = curl_easy_setopt( cHandle, CURLOPT_FAILONERROR, (void *)1 );
                        if( 0 == result )
                        {
                           result = curl_easy_setopt( cHandle, CURLOPT_NOPROGRESS, 0 );
                           if( 0 == result )
                           {
                              result = curl_easy_setopt( cHandle, CURLOPT_PROGRESSFUNCTION, progress_callback );
                              if( 0 == result )
                              {
                                 result = curl_easy_setopt( cHandle, CURLOPT_PROGRESSDATA, &pd );
                                 if( 0 == result )
                                 {
                                    result = curl_easy_perform( cHandle );
                                 }
                              }
                           }
                        }
                     }
                  }
               }
   
               curl_easy_cleanup( cHandle );
               
               if( CURLE_OK != result )
               {
                  if( errorBuf[0] )
                     errorMsg = errorBuf ;
                  else
                     errorMsg = "unidentified error" ;
               } // make sure errorMsg is filled in
            }
            else
               errorMsg = "CURLOPT_ERRORBUFFER" ;
         }
         else
            errorMsg = "Error allocating curl request" ;

         if( request.postHead_ )
            curl_formfree( request.postHead_ );

         if( ( 0 == errorMsg.size() ) && ( !(*request.cancel_) ) )
         {
            onComplete_( request, data.c_str(), data.size() );
         }
         else if( (*request.cancel_) )
         {
            onCancel_( request );
         }
         else
            onFailure_( request, errorMsg );
      }
      else
         break;
   }

   return 0 ;
}



#define NUM_READERS 4
static pthread_t            readers_[NUM_READERS];

void initializeCurlWorkers
   (  onCurlComplete_t     onComplete,
      onCurlFailure_t      onFailure,
      onCurlCancel_t       onCancel,
      onCurlSize_t         onSize,
      onCurlProgress_t     onProgress )
{
   onComplete_ = onComplete ;
   onFailure_  = onFailure  ;
   onCancel_   = onCancel   ;
   onSize_     = onSize     ;
   onProgress_ = onProgress ;
   
   getCurlRequestQueue(); // make sure it's initialized
   
   memset( readers_, -1, sizeof( readers_ ) );

   for( int i = 0 ; i < NUM_READERS ; i++ )
   {
      int create = pthread_create( readers_+i, 0, readerThread, 0 );
      if( 0 != create )
      {
         fprintf( stderr, "Error %m creating curl-reader thread\n" );
         exit( 1 );
      }
   }
}

void shutdownCurlWorkers(void)
{
   getCurlRequestQueue().abort();

   for( int i = 0 ; i < NUM_READERS ; i++ )
   {
      printf( "waiting for thread %lu\n", readers_[i] );
      void *exitStat ;
      int result ;
      if( 0 == ( result = pthread_join( readers_[i], &exitStat ) ) )
      {
         printf( "done with thread %lu\n", readers_[i] );
      }
      else
         fprintf( stderr, "Error %d(%m) waiting for curlReader thread\n", result );
   }

   printf( "curl threads stopped\n" );

}


#ifdef __STANDALONE__

static void onCurlComplete( curlRequest_t     &request,
                            void const        *data,
                            unsigned long      numRead )
{
   printf( "url %s complete: %lu bytes\n", request.url_.c_str(), numRead );
}

static void onCurlFailure( curlRequest_t     &request,
                           std::string const &errorMsg )
{
   printf( "url %s failed: %s\n", request.url_.c_str(), errorMsg.c_str() );
}

static void onCurlCancel( curlRequest_t &request )
{
   printf( "url %s cancelled\n", request.url_.c_str() );
}


static void onCurlSize( curlRequest_t &request,
                        unsigned long  size )
{
   printf( "url %s: expecting %lu bytes\n", request.url_.c_str(), size );
}

static void onCurlProgress( curlRequest_t &request,
                            unsigned long  totalReadSoFar )
{
   printf( "url %s: %lu bytes so far\n", request.url_.c_str(), totalReadSoFar );
}

int main( void )
{
   printf( "starting curl workers\n" );
   initializeCurlWorkers( onCurlComplete, onCurlFailure, onCurlCancel, onCurlSize, onCurlProgress );

   char inBuf[80];
   do {
      printf( "url: " );
      if( fgets( inBuf, sizeof( inBuf ), stdin ) )
      {
         curlRequest_t request ;
         
         request.opaque_ = 0 ;
         request.url_ = inBuf ;
         request.postHead_ = 0 ;
         bool volatile cancel = false ;
         request.cancel_ = &cancel ;

         getCurlRequestQueue().push( request );
         cancel = 1 ;
      }
      else
         break;
   } while( 1 );

   printf( "stopping curl workers\n" );
   shutdownCurlWorkers();

   return 0 ;
}


#endif
