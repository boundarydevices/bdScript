/*
 * Module ccActiveURL.cpp
 *
 * This module defines the methods of the curlCache_t class
 * as declared in ccActiveURL.h
 *
 *
 * Change History : 
 *
 * $Log: ccActiveURL.cpp,v $
 * Revision 1.3  2002-11-30 05:23:40  ericn
 * -removed (dead)lock in openHandle()
 *
 * Revision 1.2  2002/11/29 18:37:47  ericn
 * -added file:// support
 *
 * Revision 1.1  2002/11/29 16:45:27  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "ccActiveURL.h"
#include "ccDiskCache.h"
#include "ccWorker.h"
#include "parsedURL.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>

static curlCache_t *cache_ = 0 ;

void curlCache_t :: get
   ( std::string const     &url,              // input
     void                  *opaque,           // input
     curlCallbacks_t const &callbacks )       // input
{
   mutexLock_t lock( mutex_ );
   unsigned long const hash = hashURL( url );
   item_t *item = findItem( hash, url );
   if( 0 == item )
   {
      item = newItem( hash, request_t::get_, url );

      parsedURL_t parsed( url );
      if( 0 != parsed.getProtocol().compare( "file" ) )
      {
         ccDiskCache_t &diskCache = getDiskCache();
         if( diskCache.find( url.c_str(), item->diskInfo_.sequence_ ) )
         {
            item->state_ = open_ ;
            diskCache.inUse( item->diskInfo_.sequence_, item->diskInfo_.data_, item->diskInfo_.length_ );
            item->diskInfo_.useCount_ = 1 ;
            callbacks.onComplete_( opaque, item->diskInfo_.data_, item->diskInfo_.length_ );
   
            item->diskInfo_.useCount_-- ;
            if( 0 == item->diskInfo_.useCount_ )
               removeItem( item );  // done, remove item from active list
         } // found in cache... finished
         else
         {
            request_t *const req = newRequest( *item, opaque, request_t::get_, callbacks );
   
            curlTransferRequest_t workReq ;
            workReq.opaque_   = item ;
            workReq.url_      = url ;
            workReq.postHead_ = 0 ;
            workReq.cancel_   = &item->cancel_ ;
   
            getCurlRequestQueue().push( workReq );
         } // not found in cache... get it
      }
      else
      {
         std::string errMsg ;

         item->diskInfo_.fd_ = open( parsed.getPath().c_str(), O_RDONLY );
         if( 0 <= item->diskInfo_.fd_ )
         {
            off_t const fileSize = lseek( item->diskInfo_.fd_, 0, SEEK_END );
            lseek( item->diskInfo_.fd_, 0, SEEK_SET );
            void *mem = mmap( 0, fileSize, PROT_READ, MAP_PRIVATE, item->diskInfo_.fd_, 0 );
            if( MAP_FAILED != mem )
            {
               item->diskInfo_.isFile_ = true ;
               item->diskInfo_.data_   = mem ;
               item->diskInfo_.length_ = fileSize ;
            }
            else
            {
               errMsg = "map:" ;
               errMsg += parsed.getPath();
               errMsg += strerror( errno );
               close( item->diskInfo_.fd_ );
               item->diskInfo_.fd_ = - 1 ;
            }
         }
         else
         {
            errMsg = parsed.getPath();
            errMsg += ':' ;
            errMsg += strerror( errno );
         }
            
         if( 0 == errMsg.size() )
         {
            item->state_ = open_ ;
            item->diskInfo_.useCount_ = 1 ;
            callbacks.onComplete_( opaque, item->diskInfo_.data_, item->diskInfo_.length_ );
   
            item->diskInfo_.useCount_-- ;
            if( 0 == item->diskInfo_.useCount_ )
               removeItem( item );  // done, remove item from active list
         }
         else
         {
            callbacks.onFailure_( opaque, errMsg );
            removeItem( item );
         }
      }
   } // initial state
   else if( pendingCancel_ == item->state_ )
   {
      callbacks.onCancel_( opaque );
   }
   else if( retrieving_ == item->state_ )
   {
      newRequest( *item, opaque, request_t::get_, callbacks );
   }
   else
   {
      if( 0 < item->diskInfo_.useCount_ ) // or why still in cache?
      {
         item->diskInfo_.useCount_++ ;
         callbacks.onComplete_( opaque, item->diskInfo_.data_, item->diskInfo_.length_ );
   
         if( 0 == --item->diskInfo_.useCount_ )
            removeItem( item );  // done, remove item from active list
      }
      else
         fprintf( stderr, "Weird use count %lu, state %d, url %s\n", item->diskInfo_.useCount_, item->state_, item->url_.c_str() );
   } // open or pending delete, treat as immediate success
}

void curlCache_t :: post
   ( std::string const     &url,             // input
     struct HttpPost       *postHead,        // input : deallocated by worker thread. trash app-side refs
     void                  *opaque,          // input
     curlCallbacks_t const &callbacks )      // input
{
   mutexLock_t lock( mutex_ );
   unsigned long const hash = hashURL( url );
   item_t *item = findItem( hash, url );
   if( 0 == item )
   {
      item = newItem( hash, request_t::post_, url );
      request_t *const req = newRequest( *item, opaque, request_t::post_, callbacks );

      curlTransferRequest_t workReq ;
      workReq.opaque_   = item ;
      workReq.url_      = url ;
      workReq.postHead_ = postHead ;
      workReq.cancel_   = &item->cancel_ ;

      getCurlRequestQueue().push( workReq );
   } // initial state
   else if( pendingCancel_ == item->state_ )
   {
      callbacks.onCancel_( opaque );
   }
   else 
   {
      assert( 0 < item->diskInfo_.useCount_ ); // or why still in cache?
      item->diskInfo_.useCount_++ ;
      callbacks.onComplete_( opaque, item->diskInfo_.data_, item->diskInfo_.length_ );

      if( 0 == --item->diskInfo_.useCount_ )
         removeItem( item );  // done, remove item from active list
   } // open or pending delete, treat as immediate success
}

void curlCache_t :: openHandle
   ( std::string const &url,           // input
     void const       *&data,          // output
     unsigned long     &length,        // output
     unsigned long     &identifier )   // output : used to close file
{
//   mutexLock_t lock( mutex_ );
   item_t *item = findItem( hashURL( url ), url );
   assert( item && ( ( open_ == item->state_ ) || ( pendingDelete_ == item->state_ ) ) );
      
   assert( 0 < item->diskInfo_.useCount_ ); // or why still in cache?
   item->diskInfo_.useCount_++ ;
   data = item->diskInfo_.data_ ;
   length = item->diskInfo_.length_ ;
   identifier = (unsigned long)item ;
}

void curlCache_t :: closeHandle( unsigned long &identifier )
{
   mutexLock_t lock( mutex_ );
   item_t *item = (item_t *)identifier ;
   assert( 0 < item->diskInfo_.useCount_ ); // or why still in cache?
   if( 0 == --item->diskInfo_.useCount_ )
      removeItem( item );  // done, remove item from active list
}

void curlCache_t :: cancel( std::string const &url )
{
   mutexLock_t lock( mutex_ );
   unsigned long const hash = hashURL( url );
   item_t *item = findItem( hash, url );
   if( 0 != item )
   {
      if( retrieving_ == item->state_ )
      {
         item->state_  = pendingCancel_ ;
         item->cancel_ = true ;
      } // retrieving data, indicate cancel
   } // still in cache
}

void curlCache_t :: deleteURL( std::string const &url )
{
   mutexLock_t lock( mutex_ );
   unsigned long const hash = hashURL( url );
   item_t *item = findItem( hash, url );
   if( 0 != item )
   {
      if( retrieving_ == item->state_ )
      {
         item->state_  = pendingCancel_ ;
         item->cancel_ = true ;
      } // retrieving data, indicate cancel
      else if( open_ == item->state_ )
         item->state_ = pendingDelete_ ;

      item->deleteOnClose_ = true ;
   
   } // still in cache
   else
      getDiskCache().deleteFromCache( url.c_str() );
}

void curlCache_t :: transferComplete
   ( void         *request,
     void const   *data,
     unsigned long numRead )
{
   mutexLock_t lock( mutex_ );
   item_t *const item = (item_t *)request ;
   assert( ( retrieving_ == item->state_ ) || ( pendingCancel_ == item->state_ ) );
   list_head *const head = &item->requests_;
   list_head *nextReq = item->requests_.next ;
   assert( nextReq != head );

   if( retrieving_ == item->state_ )
   {
      ccDiskCache_t &diskCache = getDiskCache();
      if( diskCache.allocateInitial( item->url_.c_str(), numRead, item->diskInfo_.sequence_ ) )
      {
         //
         // store and map the data
         //
         diskCache.storeData( item->diskInfo_.sequence_,
                              numRead,
                              data, 
                              item->deleteOnClose_ );
         diskCache.inUse( item->diskInfo_.sequence_, item->diskInfo_.data_, item->diskInfo_.length_ );
         assert( item->diskInfo_.length_ == numRead );

         item->state_ = open_ ;

         while( nextReq != head )
         {
            request_t *req = (request_t *)nextReq ;

            ++item->diskInfo_.useCount_ ;
            req->callbacks_.onComplete_( req->opaque_, item->diskInfo_.data_, item->diskInfo_.length_ );
            --item->diskInfo_.useCount_ ;

            nextReq = req->chain_.next ;
            removeRequest( *req );
         }

         if( 0 == item->diskInfo_.useCount_ )
            removeItem( item );  // done, remove item from active list
      }
      else
      {
         //
         // disk cache failure
         //
         std::string const errorMsg = "out of disk space" ;

         while( nextReq != head )
         {
            request_t *req = (request_t *)nextReq ;

            req->callbacks_.onFailure_( req->opaque_, errorMsg );

            nextReq = req->chain_.next ;
            removeRequest( *req );
         }
         
         removeItem( item );  // done, remove item from active list
      }
   }
   else
   {
      while( nextReq != head )
      {
         request_t *req = (request_t *)nextReq ;
         req->callbacks_.onCancel_( req->opaque_ );
         nextReq = req->chain_.next ;
         removeRequest( *req );
      }
      removeItem( item );  // done, remove item from active list
   } // pending cancel... cancel all requests
}

void curlCache_t :: transferFailed
   ( void              *request,
     std::string const &errorMsg )
{
   mutexLock_t lock( mutex_ );
   item_t *const item = (item_t *)request ;
   if( ( retrieving_ == item->state_ ) || ( pendingCancel_ == item->state_ ) )
   {
      list_head *const head = &item->requests_;
      list_head *nextReq = item->requests_.next ;
      assert( nextReq != head );
      
      while( nextReq != head )
      {
         request_t *req = (request_t *)nextReq ;
         req->callbacks_.onFailure_( req->opaque_, errorMsg );
         nextReq = req->chain_.next ;
         removeRequest( *req );
      }
      removeItem( item );  // done, remove item from active list
   }
   else
   {
      fprintf( stderr, "Weird state %d in transferFailed\n", item->state_ );
      exit( 1 );
   }
}

void curlCache_t :: transferCancelled( void *request )
{
   mutexLock_t lock( mutex_ );
   item_t *const item = (item_t *)request ;
   if( ( retrieving_ == item->state_ ) || ( pendingCancel_ == item->state_ ) )
   {
      assert( ( retrieving_ == item->state_ ) || ( pendingCancel_ == item->state_ ) );
      list_head *const head = &item->requests_;
      list_head *nextReq = item->requests_.next ;
      assert( nextReq != head );
      
      while( nextReq != head )
      {
         request_t *req = (request_t *)nextReq ;
         req->callbacks_.onCancel_( req->opaque_ );
         nextReq = req->chain_.next ;
         removeRequest( *req );
      }
      removeItem( item );  // done, remove item from active list
   }
   else
   {
      fprintf( stderr, "Weird state %d in transferCancelled\n", item->state_ );
      exit( 1 );
   }
}

void curlCache_t :: transferFileSize
   ( void         *request,
     unsigned long size )
{
   mutexLock_t lock( mutex_ );
   item_t *const item = (item_t *)request ;
   assert( ( retrieving_ == item->state_ ) || ( pendingCancel_ == item->state_ ) );
   list_head *const head = &item->requests_;
   list_head *nextReq = item->requests_.next ;
   assert( nextReq != head );
   
   while( nextReq != head )
   {
      request_t *req = (request_t *)nextReq ;
      req->callbacks_.onSize_( req->opaque_, size );
      nextReq = req->chain_.next ;
   }
}

void curlCache_t :: transferProgress
   ( void         *request,
     unsigned long numReadSoFar )
{
   mutexLock_t lock( mutex_ );
   item_t *const item = (item_t *)request ;
   assert( ( retrieving_ == item->state_ ) || ( pendingCancel_ == item->state_ ) );
   list_head *const head = &item->requests_;
   list_head *nextReq = item->requests_.next ;
   assert( nextReq != head );
   
   while( nextReq != head )
   {
      request_t *req = (request_t *)nextReq ;
      req->callbacks_.onProgress_( req->opaque_, numReadSoFar );
      nextReq = req->chain_.next ;
   }
}

curlCache_t :: curlCache_t( void )
{
   for( unsigned i = 0 ; i < numHashBuckets_ ; i++ )
      INIT_LIST_HEAD( &hash_[i] );
}

curlCache_t :: ~curlCache_t( void )
{
}

unsigned long curlCache_t :: hashURL( std::string const &url )
{
   char const *s = url.c_str();
   unsigned long hash = 0 ;
   while( *s )
      hash += *s++ ;
   return hash & hashMask_ ;
}

curlCache_t :: item_t *curlCache_t :: findItem
   ( unsigned long      hash,
     std::string const &url )
{
   list_head *const head = &hash_[ hash ];
   list_head *next = head->next ;
   for( ; next != head ; next = next->next )
   {
      item_t *item = (item_t *)next ;
      if( 0 == url.compare( item->url_ ) )
         return item ;
   }

   return 0 ;
}

curlCache_t :: item_t *curlCache_t :: newItem
   ( unsigned long          hash,
     request_t :: type_e    type,
     std::string const     &url )
{
   item_t * const newItem = new item_t ;
   INIT_LIST_HEAD( &newItem->chainHash_ );

   newItem->url_           = url ;
   newItem->state_         = retrieving_ ;
   newItem->cancel_        = false ;
   newItem->deleteOnClose_ = (request_t::post_ == type );
   INIT_LIST_HEAD( &newItem->requests_ );
   memset( &newItem->diskInfo_, 0, sizeof( newItem->diskInfo_ ) );
   list_add_tail( &newItem->chainHash_, &hash_[hash] );

   return newItem ;
}

void curlCache_t :: removeItem( item_t *item )
{
   assert( 0 == item->diskInfo_.useCount_ );
   if( ( pendingDelete_ == item->state_ )
       ||
       ( open_ == item->state_ ) )
   {
      if( !item->diskInfo_.isFile_ )
      {
         ccDiskCache_t &diskCache = getDiskCache();
         diskCache.notInUse( item->diskInfo_.sequence_ );
         if( ( pendingDelete_ == item->state_ )
             ||
             ( item->deleteOnClose_ ) )
            diskCache.deleteFromCache( item->url_.c_str() );
      }
      else
      {
         int result = munmap( (void *)item->diskInfo_.data_, item->diskInfo_.length_ );
         if( 0 != result )
            fprintf( stderr, "Error unmapping file:// object\n" );
         result = close( item->diskInfo_.fd_ );
         if( 0 != result )
            fprintf( stderr, "Error closing file:// object\n" );

         item->diskInfo_.data_ = 0 ;
         item->diskInfo_.length_ = 0 ;
         item->diskInfo_.fd_ = -1 ;
      }
   }

   list_del( &item->chainHash_ );
   delete item ;
}

curlCache_t :: request_t *curlCache_t :: newRequest
   ( item_t                &item,
     void                  *opaque,
     request_t :: type_e    type,
     curlCallbacks_t const &callbacks )
{
   request_t * const req = new request_t ;
   INIT_LIST_HEAD( &req->chain_ );
   req->opaque_    = opaque ;
   req->type_      = type ;
   req->callbacks_ = callbacks ;
   list_add_tail( &req->chain_, &item.requests_ );

   return req ;
}

void curlCache_t :: removeRequest( request_t &req )
{
   list_del( &req.chain_ );
   delete &req ;
}

static void onCurlComplete( curlTransferRequest_t  &request,
                            void const             *data,
                            unsigned long           numRead )
{
   getCurlCache().transferComplete( request.opaque_, data, numRead );
}

static void onCurlFailure
   ( curlTransferRequest_t &request,
     std::string const     &errorMsg )
{
   getCurlCache().transferFailed( request.opaque_, errorMsg );
}

static void onCurlCancel( curlTransferRequest_t &request )
{
   getCurlCache().transferCancelled( request.opaque_ );
}


static void onCurlSize
   ( curlTransferRequest_t &request,
     unsigned long          size )
{
   getCurlCache().transferFileSize( request.opaque_, size );
}

static void onCurlProgress
   ( curlTransferRequest_t &request,
     unsigned long          totalReadSoFar )
{
   getCurlCache().transferProgress( request.opaque_, totalReadSoFar );
}
   
curlCache_t &getCurlCache( void )
{
   if( 0 == cache_ )
   {
      initializeCurlWorkers( onCurlComplete, onCurlFailure, onCurlCancel, onCurlSize, onCurlProgress );
      getDiskCache(); // initialize
      cache_ = new curlCache_t ;
   }

   return *cache_ ;
}


#ifdef __STANDALONE__

#include <unistd.h>
#include <ctype.h>

void curlCacheComplete( void         *opaque,
                        void const   *data,
                        unsigned long numRead )
{
   printf( "complete, %lu bytes\n", numRead );
//   fwrite( data, 1, numRead, stdout );
}

void curlCacheFailure( void              *opaque,
                       std::string const &errorMsg )
{
   printf( "failed:%s\n", errorMsg.c_str() );
}

void curlCacheCancel( void *opaque )
{
   printf( "cancelled\n" );
}

void curlCacheSize( void         *opaque,
                    unsigned long size )
{
   printf( "fileSize:%lu bytes\n", size );
}

void curlCacheProgress( void         *opaque,
                        unsigned long totalReadSoFar )
{
   printf( "progress:%lu bytes so far\n", totalReadSoFar );
}


static void printPostParams( struct HttpPost *head )
{
   while( head )
   {
      printf( "name=%s(length %u)\n", head->name, head->namelength );
      printf( "contents=" );
      if( head->contentslength )
         fwrite( head->contents, head->contentslength, 1, stdout );
      else
         printf( "%s", head->contents );
      
      printf( "(length %ld)\n", head->contentslength );
      printf( "buffer=" );
      if( head->bufferlength )
         fwrite( head->buffer, head->bufferlength, 1, stdout );
      printf( "(length %ld)\n", head->bufferlength );

      if( head->contenttype )
         printf( "content-type=%s\n", head->contenttype );
      else
         printf( "no content type\n" );

      if( head->contentheader )
         printf( "have content headers\n" );
      else
         printf( "no content headers\n" );

      if( head->more )
         printf( "have more\n" );
      else
         printf( "no more\n" );

      printf( "flags %lx\n", head->flags );
      if( 0 != head->showfilename )
         printf( "filename %s\n", head->showfilename );
      else
         printf( "no showfilename\n" );

      head = head->next ;
   }
}

int main( void )
{
   printf( "initializing curl cache\n" );
   curlCache_t &cache = getCurlCache();

   curlCallbacks_t callbacks ;

   callbacks.onComplete_ = curlCacheComplete ;
   callbacks.onFailure_  = curlCacheFailure  ;
   callbacks.onCancel_   = curlCacheCancel   ;
   callbacks.onSize_     = curlCacheSize     ;
   callbacks.onProgress_ = curlCacheProgress ;

   char inBuf[256];
   while( fgets( inBuf, sizeof( inBuf ), stdin ) )
   {
      char *end = inBuf + strlen( inBuf ) - 1 ;
      while( end >= inBuf )
      {
         if( iscntrl( *end ) )
            *end-- = '\0' ;
         else
            break;
      }

      char *inBufPtr ;
      char const *cmd = strtok_r( inBuf, " \t", &inBufPtr );
      if( cmd )
      {
         char cmdBuf[sizeof(inBuf)];
         strcpy( cmdBuf, cmd );
         char const *url = strtok_r( 0, " \t", &inBufPtr );
         if( url )
         {
            if( 0 == strcasecmp( cmdBuf, "get" ) )
            {
               cache.get( url, 0, callbacks );

            }
            else if( 0 == strcasecmp( cmdBuf, "post" ) )
            {
               char urlBuf[sizeof(inBuf)];
               strcpy( urlBuf, url );

               struct HttpPost* paramHead = NULL;
               struct HttpPost* paramTail = NULL;

               char *param ;
               while( 0 != ( param = strtok_r( 0, " \t", &inBufPtr ) ) )
               {
                  char *paramPtr ;
                  char const *name = strtok_r( param, "=", &paramPtr );
                  if( name )
                  {
                     char nameBuf[sizeof( inBuf)];
                     strcpy( nameBuf, name );
                     char const *value = strtok_r( 0, "=", &paramPtr );
                     if( value )
                     {
                        if( '@' != value[0] )
                        {
                           curl_formadd( &paramHead, &paramTail, 
                                         CURLFORM_COPYNAME, nameBuf,
                                         CURLFORM_COPYCONTENTS, value,
                                         CURLFORM_END );
                           continue;
                        }
                        else
                        {
                           curl_formadd( &paramHead, &paramTail, 
                                         CURLFORM_COPYNAME, nameBuf,
                                         CURLFORM_FILE, value+1,
                                         CURLFORM_END );
                           continue;
                        }
                     }
                  }
                  fprintf( stderr, "Invalid param (format is name=value)\n" );
               }
//               printPostParams( paramHead );
               cache.post( url, paramHead, 0, callbacks );
            }
            else if( 0 == strcasecmp( cmdBuf, "cancel" ) )
            {
               cache.cancel( url );
            }
            else if( 0 == strcasecmp( cmdBuf, "delete" ) )
            {
               cache.deleteURL( url );
            }
            else
               fprintf( stderr, "Usage: get|post|cancel|delete url\n" );
         }
         else
            fprintf( stderr, "Usage: get|post|cancel|delete url\n" );

      }
      else
         fprintf( stderr, "Usage: get|put|cancel|delete url\n" );
   }

   printf( "stopping curl workers\n" );
   shutdownCurlWorkers();

   printf( "shutting down disk cache\n" );
   shutdownCCDiskCache();

   return 0 ;
}

#endif
