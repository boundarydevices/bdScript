#ifndef __CURLCACHE_H__
#define __CURLCACHE_H__ "$Id: curlCache.h,v 1.2 2002-10-06 14:52:02 ericn Exp $"

/*
 * curlCache.h
 *
 * This header file declares the curlCache_t class, which
 * is used to keep track of recently used files retrieved
 * through curl.
 *
 *
 * Change History : 
 *
 * $Log: curlCache.h,v $
 * Revision 1.2  2002-10-06 14:52:02  ericn
 * -made getCachedName() public
 *
 * Revision 1.1.1.1  2002/09/28 16:50:46  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <stdio.h>
#include <curl/curl.h>
#include <string>

class curlFile_t {
public:
   ~curlFile_t( void );
   curlFile_t( curlFile_t const & );

   //
   // primary functions
   //
   inline bool          isOpen( void ) const { return 0 <= fd_ ; }
   inline unsigned long getSize( void ) const { return size_ ; }
   inline void const   *getData( void ) const { return data_ ; }

   //
   // HTTP header functions
   //
   inline char const *getEffectiveURL( void ) const { return effective_ ; }      // CURLINFO_EFFECTIVE_URL
   inline long        getHttpCode( void ) const { return httpCode_ ; }           // CURLINFO_HTTP_CODE 
   inline long        getFileTime( void ) const { return fileTime_ ; }           // CURLINFO_FILETIME
   inline char const *getMimeType( void ) const { return mimeType_ ; }           // CURLINFO_CONTENT_TYPE

private:
   curlFile_t( char const *tmpFileName );

   struct header_t {
      unsigned long  size_ ;
      long           httpCode_ ;
      long           fileTime_ ;
   };

   friend class curlCache_t ;
   friend class urlFile_t ;

   int            fd_ ;
   unsigned long  fileSize_ ;
   unsigned long  size_ ;
   void const    *data_ ;
   char const    *effective_ ;
   long           httpCode_ ;
   long           fileTime_ ;
   long           transferTime_ ; 
   char const    *mimeType_ ;
   
   friend bool findEnd( void const   *mem,            // input : mapped file
                        unsigned long fileSize,       // input : size of mapped memory
                        unsigned long dataSize,       // input : size of content portion
                        char const  *&effectiveURL,   // output : effective url
                        char const  *&mimeType );     // output : mime type
};


class curlCache_t {
public:
   //
   // returns open curlFile_t if found in cache or retrieved from server
   //
   curlFile_t get( char const url[] );

   std::string getCachedName( char const url[] );

private:
   curlCache_t( curlCache_t const & ); // no copies
   curlCache_t( char const    *dirName,
                unsigned long  maxSize,
                unsigned long  maxEntries );
   ~curlCache_t( void );

   friend curlCache_t &getCurlCache( void );
   void makeSpace( unsigned numFiles, unsigned long bytes );

   //
   // use this function to start a transfer
   //
   bool startTransfer( char const targetURL[],
                       CURL     *&cHandle,
                       int       &fd );

   //
   // use this function to complete a transfer after curl_easy_perform()
   //
   bool store( char const targetURL[],
               CURL      *curl,   // handle to connection
               int        fd );

   //
   // use this function to discard a transfer
   //
   void discard( char const targetURL[],
                 CURL      *curl,
                 int        fd );

   char const * const   dirName_ ;
   unsigned long const  maxSize_ ;
   unsigned long const  maxEntries_ ;
   long                 curlTimeBuffer_ ;
   long                 transTimeBuffer_ ;
};

curlCache_t &getCurlCache( void );


#endif

