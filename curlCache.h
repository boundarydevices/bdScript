#ifndef __CURLCACHE_H__
#define __CURLCACHE_H__ "$Id: curlCache.h,v 1.7 2002-10-31 02:08:55 ericn Exp $"

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
 * Revision 1.7  2002-10-31 02:08:55  ericn
 * -added default constructor for curlRequest_t
 *
 * Revision 1.6  2002/10/24 13:18:22  ericn
 * -modified for relative URLs
 *
 * Revision 1.5  2002/10/13 14:36:51  ericn
 * -made cache usage optional
 *
 * Revision 1.4  2002/10/13 13:42:09  ericn
 * -got rid of content reference for file posts
 *
 * Revision 1.3  2002/10/09 01:10:03  ericn
 * -added post support
 *
 * Revision 1.2  2002/10/06 14:52:02  ericn
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
#include <vector>

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



class curlRequest_t {
public:
   curlRequest_t( char const url[] );
   curlRequest_t( void );
   ~curlRequest_t( void );

   struct param_t {
      bool        isFile_ ;
      char const *name_ ;
      union {
         char const *stringValue_ ;
         char const *fileValue_ ;
      } value_ ;
   };

   //
   // parameters to these routines must stay around until completion
   // of curlCache_t::get(request) or until destructor of curlRequest_t
   //
   void addVariable( char const *name,
                     char const *value );

   void addFile( char const   *name,
                 char const   *path );

   bool hasFile( void ) const { return hasFile_ ; }

   char const *getURL( void ) const { return url_.c_str(); }

private:
   friend class curlCache_t ;

   std::string             url_ ;
   bool                    hasFile_ ;
   std::vector<param_t>    parameters_ ;
};



class curlCache_t {
public:
   //
   // Use this routine for simple get()'s
   // 
   // To build up a request with multiple parameters, use
   // curlRequest_t
   //
   curlFile_t get( char const url[], bool useCache = true );
   std::string getCachedName( char const url[] );

   //
   // use this routine to build up the parts before a post request
   //
   curlFile_t post( curlRequest_t const &, bool useCache = false );
   std::string getCachedName( curlRequest_t const & );

private:
   curlCache_t( curlCache_t const & ); // no copies
   curlCache_t( char const    *dirName,
                unsigned long  maxSize,
                unsigned long  maxEntries );
   ~curlCache_t( void );

   friend curlCache_t &getCurlCache( void );
   void makeSpace( unsigned numFiles, unsigned long bytes );

   //
   // use this function to start a get transfer
   //
   bool startTransfer( std::string const &cachedName,
                       char const        *targetURL,
                       CURL              *&cHandle,
                       int               &fd );

   //
   // Use this function to start a post transfer.
   // If this routine returns true, the caller must clean up the
   // output variables:
   //
   //    close() the fd
   //    curl_easy_cleanup() the cHandle
   //    curl_formfree() the postHead
   //    curl_slist_free_all() the headerlist
   //
   bool startPost( std::string const   &cachedName,   // input
                   curlRequest_t const &req,          // input
                   CURL               *&cHandle,      // output
                   int                 &fd,           // output
                   struct curl_slist  *&headerlist,   // output 
                   struct HttpPost    *&postHead );   // output

   //
   // use this function to complete a transfer after curl_easy_perform()
   //
   bool store( CURL      *curl,   // handle to connection
               int        fd );

   //
   // use this function to discard a transfer
   //
   void discard( std::string const &cacheName,
                 CURL              *curl,
                 int                fd );

   char const * const   dirName_ ;
   unsigned long const  maxSize_ ;
   unsigned long const  maxEntries_ ;
   long                 curlTimeBuffer_ ;
   long                 transTimeBuffer_ ;
};

curlCache_t &getCurlCache( void );


#endif

