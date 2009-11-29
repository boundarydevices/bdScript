#ifndef __CCDISKCACHE_H__
#define __CCDISKCACHE_H__ "$Id: ccDiskCache.h,v 1.5 2006-12-01 18:32:39 tkisky Exp $"

/*
 * ccDiskCache.h
 *
 * This header file declares the ccDiskCache_t
 * class, which is the disk cache portion of the
 * curl cache subsystem. 
 *
 * See curlCache.txt for an overview of the design.
 *
 *
 * Change History : 
 *
 * $Log: ccDiskCache.h,v $
 * Revision 1.5  2006-12-01 18:32:39  tkisky
 * -friend function definition
 *
 * Revision 1.4  2003/12/06 22:06:37  ericn
 * -added support for temp file and offset
 *
 * Revision 1.3  2003/08/01 14:28:35  ericn
 * -modified to return status of storeData
 *
 * Revision 1.2  2002/11/30 05:24:18  ericn
 * -modified to prevent copies of header_t
 *
 * Revision 1.1  2002/11/26 23:28:06  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <string.h>
#include <map>
#include <string>
#include "dlList.h"
#include <vector>

class ccDiskCache_t {
public:
   //
   // find an item in the cache, return sequence number for use in marking
   // note that caller (curl cache) should check temporary entries before
   // calling this.
   //
   bool find( char const *url,
              unsigned   &sequence );

   //
   // Flag usage status of the file. Note that this call 
   // must immediately follow a successful call to either 
   // find() or storeData().
   //
   // returns pointer and length to item's data
   //
   void inUse( unsigned       sequence,
               void const   *&data,
               unsigned long &length );

   //
   // Get offset of data within file. Note that this is inherently unsafe and
   // should only be used by trained professionals (and then only when the
   // cacheing program is shutting down!)
   //
   bool getDataOffset( unsigned sequence, unsigned &offset );

   //
   // Tell disk cache that an item is no longer in use
   //
   void notInUse( unsigned sequence );

   //
   // delete an item from the cache. Returns true if found
   // and deleted, false otherwise
   //
   bool deleteFromCache( char const *url );

   //
   // allocate a cache entry, reserving some initial space
   // returns true if space is made available
   //
   bool allocateInitial( char const *url,               // input
                         unsigned    initialSpace,      // input
                         unsigned   &sequence );        // output

   //
   // increase the allocation for an unnamed cache item
   //
   bool allocateMore( unsigned sequence,              // input
                      unsigned howMuchMore );         // input

   //
   // free up space allocated for aborted or failed transfer
   //
   void discardTemp( unsigned sequence );

   //
   // commit the temporary item, writing it to disk
   //
   bool storeData( unsigned    sequence,        // input : returned from allocateInitial
                   unsigned    size,            // input : how much data to store. Must be less than allocation
                   void const *data,            // input : data to write
                   bool        temporary );

   //
   // debug routines and internals
   //
   struct header_t {
      list_head      chain_ ;
      unsigned       sequence_ ;
      unsigned long  size_ ;
      int            fd_ ;        // true when opened
      void const    *data_ ;      // mmap'd data
      bool           completed_ ; // true when successfully stored
      bool           temporary_ ; // if true, removed in notInUse()
      char           name_[1];    // actually NULL-terminated string
      header_t( void ){ memset( this, 0, sizeof( this ) ); }
   private:
      header_t( header_t const & ); // no copies
   };

   // size of committed cached entries
   unsigned long committedSize( void ) const { return currentSize_ ; }

   // size being held for in-progress transfers
   unsigned long reservedSize( void ) const { return reservedSize_ ; }

   // number of cache entries
   unsigned long numCacheEntries( void ) const { return cacheEntries_.size(); }

   // sequence number of next item in cache
   unsigned nextSequenceNumber( void ) const { return nextSequence_ ; }

   // dump cache content to stdout
   void dump( void ) const ;

   void retrieveURLs( std::vector<std::string> &urls ) const ;

   std::string constructName( unsigned long sequence ) const ;

private:
   ccDiskCache_t( ccDiskCache_t const & ); // no copies
   ccDiskCache_t( char const    *dirName,
                  unsigned long  maxSize );
   ~ccDiskCache_t( void );

   //
   // Only one. Use this call to get it
   //
   friend ccDiskCache_t &getDiskCache( void );
   friend void shutdownCCDiskCache( void );

   typedef std::map<unsigned,header_t *> bySequence_t ;
   typedef std::map<std::string,unsigned> byName_t ;

   bool makeSpace( unsigned long spaceNeeded );
   void freeOne( header_t &header );

   std::string   basePath_ ; // includes trailing '/'
   unsigned long maxSize_ ;
   unsigned long currentSize_ ;
   unsigned long reservedSize_ ;
   unsigned      nextSequence_ ;
   bySequence_t  cacheEntries_ ;
   byName_t      entriesByName_ ;
   list_head     mru_ ;
};

void shutdownCCDiskCache( void );
ccDiskCache_t &getDiskCache( void );


#endif

