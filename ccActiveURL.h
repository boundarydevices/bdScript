#ifndef __CCACTIVEURL_H__
#define __CCACTIVEURL_H__ "$Id: ccActiveURL.h,v 1.6 2003-12-06 22:06:22 ericn Exp $"

/*
 * ccActiveURL.h
 *
 * This header file declares the primary interfaces to
 * the active URL cache of the curl cache subsystem. 
 *
 * Refer to curlCache.txt for more description and 
 * musing than you can probably stomach.
 *
 * The curlCache_t class is the primary object and corresponds
 * to the Active URL cache as described in the er, document.
 *
 * Change History : 
 *
 * $Log: ccActiveURL.h,v $
 * Revision 1.6  2003-12-06 22:06:22  ericn
 * -added support for temp file and offset
 *
 * Revision 1.5  2003/08/01 14:27:24  ericn
 * -added addRef, deref
 *
 * Revision 1.4  2002/12/02 15:07:51  ericn
 * -added callback param for file handle
 *
 * Revision 1.3  2002/11/30 00:53:43  ericn
 * -changed name to semClasses.h
 *
 * Revision 1.2  2002/11/29 18:37:43  ericn
 * -added file:// support
 *
 * Revision 1.1  2002/11/29 16:45:27  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "semClasses.h"
#include <string>
#include <curl/curl.h>
#include "dlList.h"

typedef void (*curlCacheComplete_t)( void         *opaque,
                                     void const   *data,
                                     unsigned long numRead,
                                     unsigned long handle ); // use this to remove reference
typedef void (*curlCacheFailure_t)( void              *opaque,
                                    std::string const &errorMsg );
typedef void (*curlCacheCancel_t)( void *opaque );
typedef void (*curlCacheSize_t)( void         *opaque,
                                 unsigned long size );
typedef void (*curlCacheProgress_t)( void         *opaque,
                                     unsigned long totalReadSoFar );

struct curlCallbacks_t {
   curlCacheComplete_t     onComplete_ ;
   curlCacheFailure_t      onFailure_ ;
   curlCacheCancel_t       onCancel_ ;
   curlCacheSize_t         onSize_ ;
   curlCacheProgress_t     onProgress_ ;
};

class curlCache_t {
public:

   // -----------------------
   // app-side calls
   //
   void get( std::string const     &url,              // input
             void                  *opaque,           // input
             curlCallbacks_t const &callbacks );      // input

   void post( std::string const     &url,             // input
              struct HttpPost       *postHead,        // input : deallocated by worker thread. trash app-side refs
              void                  *opaque,          // input
              curlCallbacks_t const &callbacks );     // input

   void openHandle( std::string const &url,           // input
                    void const       *&data,          // output
                    unsigned long     &length,        // output
                    unsigned long     &identifier );  // output : used to close file
   void addRef( unsigned long identifier );    // input : also used later to close file
   void closeHandle( unsigned long identifier );
   bool deref( unsigned long  identifier,
               void const   *&data,
               unsigned long &length );
   
   // Be very careful with these calls. The cache can change (or move) from
   // under you if additional files are retrieved, and the
   //
   void getTempFileName( unsigned long  identifier,   // input
                         std::string   &localName );  // output: be careful with this one... file can go away!
   void getDataOffset( unsigned long identifier,      // input
                       unsigned     &offset );        // output

   void cancel( std::string const &url ); // cancel all
   void deleteURL( std::string const &url );

   // -----------------------
   // worker-side calls
   //
   void transferComplete( void         *request,
                          std::string  &data );
   void transferFailed( void              *request,
                        std::string const &errorMsg );
   void transferCancelled( void *request );
   void transferWrite( void         *request,
                       void const   *data,
                       unsigned long size );
   void transferFileSize( void         *request,
                          unsigned long size );
   void transferProgress( void         *request,
                          unsigned long numReadSoFar );

private:
   struct request_t {
      list_head         chain_ ;
      enum type_e {
         get_,
         post_
      }                 type_ ;
      void             *opaque_ ;
      curlCallbacks_t   callbacks_ ;
   };

   struct diskInfo_t {
      unsigned       sequence_ ;
      void const    *data_ ;
      unsigned long  length_ ;
      unsigned long  useCount_ ;
      bool           isFile_ ;
      int            fd_ ;
   };

   enum state_e {
      retrieving_,
      pendingCancel_,
      open_,
      pendingDelete_
   };

   struct item_t {
      list_head    chainHash_ ;
      std::string  url_ ;
      state_e      state_ ;
      list_head    requests_ ;
      bool         deleteOnClose_ ;
      diskInfo_t   diskInfo_ ;
      bool         cancel_ ;  // polled by worker thread to indicate cancel
   };

   curlCache_t( void );
   ~curlCache_t( void );

   static unsigned long hashURL( std::string const &url );

   item_t *findItem( unsigned long      hash,
                     std::string const &url );
   item_t *newItem( unsigned long          hash,
                    request_t :: type_e    type,
                    std::string const     &url );
   void removeItem( item_t *item );

   request_t *newRequest( item_t                &item,
                          void                  *opaque,
                          request_t :: type_e    type,
                          curlCallbacks_t const &callbacks );
   void removeRequest( request_t &req );

   friend curlCache_t &getCurlCache( void );
   
   enum {
      numHashBuckets_ = 32,
      hashMask_       = 31
   };
   
   mutex_t     mutex_ ;
   list_head   hash_[numHashBuckets_];
};

#endif

