/*
 * Module ccDiskCache.cpp
 *
 * This module defines the methods of the ccDiskCache_t
 * class as declared in ccDiskCache.h and described in
 * curlCache.txt.
 *
 *
 * Change History : 
 *
 * $Log: ccDiskCache.cpp,v $
 * Revision 1.4  2003-08-01 14:28:05  ericn
 * -fixed urlLen in initial cache load
 *
 * Revision 1.3  2003/07/31 04:30:52  ericn
 * -modified to longword align data (by padding header)
 *
 * Revision 1.2  2002/11/29 16:44:54  ericn
 * -removed error message for normal occurrence
 *
 * Revision 1.1  2002/11/26 23:28:06  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "ccDiskCache.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <dirent.h>
#include "memFile.h"

#ifdef __STANDALONE__
   struct abort_t {
      int status_ ;
   };
   
   #define ABORT( val ) { abort_t abort ; abort.status_ = val ; throw( abort ); }
#else
   #define ABORT exit
#endif

static std::string getCacheName( std::string const &baseName,
                                 unsigned long      sequence )
{
   std::string rval ;
   rval.reserve( baseName.size() + 9 );
   char cTemp[9];
   sprintf( cTemp, "%08lx", sequence );
   rval = baseName + cTemp ;

   return rval ;
}

bool ccDiskCache_t :: find
   ( char const *url,
     unsigned   &sequence )
{
   sequence = 0 ;

   std::string const &s( url );
   byName_t::const_iterator it = entriesByName_.find( s );
   if( entriesByName_.end() != it )
   {
      sequence = (*it).second ;
      bySequence_t :: const_iterator seqIt = cacheEntries_.find( sequence );
      if( seqIt != cacheEntries_.end() )
      {
         header_t &header = *((*seqIt).second);
         if( header.completed_ )
         {
            // unchain and move to head
            list_del( &header.chain_ );
            list_add( &header.chain_, &mru_ );
            return true ;
         }
         else
         {
            fprintf( stderr, "%s:%s:request for incomplete file, sequence %lu, url %s\n", __FILE__,
                     __PRETTY_FUNCTION__, sequence, url );
            ABORT( 1 );
         }
      }
      else
      {
         fprintf( stderr, "%s:%s:Cache not consistent for sequence %lu, url %s\n", __FILE__,
                  __PRETTY_FUNCTION__, sequence, url );
         ABORT( 1 );
      }
   }
      
   return false ;
}

void ccDiskCache_t :: inUse
   ( unsigned       sequence,
     void const   *&data,
     unsigned long &length )
{
   bySequence_t :: const_iterator it = cacheEntries_.find( sequence );
   if( it != cacheEntries_.end() )
   {
      header_t &header = *((*it).second);
      if( header.completed_ && ( 0 > header.fd_ ) && ( 0 == header.data_ ) )
      {
         std::string const &cachedFileName = getCacheName( basePath_, sequence );
         header.fd_ = open( cachedFileName.c_str(), O_RDONLY );
         if( 0 <= header.fd_ )
         {
            unsigned const urlLen = strlen( header.name_ ) + 1 ;
            unsigned const pad = ( 4 - ( urlLen & 3 ) ) & 3 ;
            unsigned const dataOffs = urlLen + pad + sizeof( urlLen ) + sizeof( header.size_ );
            
            off_t const fileSize = lseek( header.fd_, 0, SEEK_END );
            if( fileSize == dataOffs + header.size_ )
            {
               lseek( header.fd_, dataOffs, SEEK_SET );
               void *mem = mmap( 0, fileSize, PROT_READ, MAP_PRIVATE, header.fd_, 0 );
               if( MAP_FAILED != mem )
               {
                  header.data_   = mem ;
                  data   = (char *)mem + dataOffs ;
                  length = header.size_ ;
               }
               else
               {
                  fprintf( stderr, "%s:%s: error mapping file %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
                  ABORT( 1 );
               }
            }
            else
            {
               fprintf( stderr, "%s:%s: file %lu too short\n", __FILE__, __PRETTY_FUNCTION__, sequence );
               ABORT( 1 );
            }
         }
         else
         {
            fprintf( stderr, "%s:%s: error opening object %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
            ABORT( 1 );
         }
      }
      else
      {
         fprintf( stderr, "%s:%s: attempt to re-allocate object %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
         fprintf( stderr, "header %s, fd %d, data %p\n", header.completed_ ? "completed" : "incomplete",
                          header.fd_, header.data_ );
         ABORT( 1 );
      }
   }
   else
   {
      fprintf( stderr, "%s:%s: Invalid sequence %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
      ABORT( 1 );
   }
}

void ccDiskCache_t :: notInUse( unsigned sequence )
{
   bySequence_t :: const_iterator it = cacheEntries_.find( sequence );
   if( it != cacheEntries_.end() )
   {
      header_t &header = *((*it).second);
      if( header.completed_ && ( 0 <= header.fd_ ) && ( 0 != header.data_ ) )
      {
         unsigned const urlLen = strlen( header.name_ );
         unsigned const dataOffs = urlLen + sizeof( urlLen ) + sizeof( header.size_ );
         if( 0 != munmap( (void *)header.data_, header.size_ + dataOffs ) )
         {
            fprintf( stderr, "%s:munmap:%m\n", header.name_ );
            ABORT( 1 );
         }

         header.data_ = 0 ;

         if( 0 != close( header.fd_ ) )
         {
            fprintf( stderr, "%s:close:%m\n", header.name_ );
            ABORT( 1 );
         }

         header.fd_ = -1 ;

         if( header.temporary_ )
            freeOne( header );
      }
      else
      {
         fprintf( stderr, "%s:%s Attempt to free unused file %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
         ABORT( 1 );
      }
   }
   else
   {
      fprintf( stderr, "%s:%s Invalid sequence %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
      ABORT( 1 );
   }
}

bool ccDiskCache_t :: deleteFromCache( char const *url )
{
   unsigned sequence ;
   if( find( url, sequence ) )
   {
      bySequence_t :: const_iterator seqIt = cacheEntries_.find( sequence );
      if( seqIt != cacheEntries_.end() )
      {
         header_t &header = *((*seqIt).second);
         if( header.completed_ && ( 0 > header.fd_ ) && ( 0 == header.data_ ) )
         {
            freeOne( header );
            return true ;
         }
         else
            fprintf( stderr, "Can't delete %lu:%s: file in use\n", sequence, url );
      }
      else
         fprintf( stderr, "Error getting sequence %lu:%s for deletion\n", sequence, url );
   }

   return false ;
}

bool ccDiskCache_t :: allocateInitial
   ( char const *url,               // input
     unsigned    initialSpace,      // input
     unsigned   &sequence )         // output
{
   sequence = 0 ;

   std::string const &s( url );
   byName_t::const_iterator it = entriesByName_.find( s );
   if( entriesByName_.end() == it )
   {
      unsigned long const freeSpace = maxSize_ - currentSize_ - reservedSize_ ;
      if( ( initialSpace <= freeSpace ) || makeSpace( initialSpace-freeSpace ) )
      {
         reservedSize_ += initialSpace ;

         unsigned const urlLen = strlen( url );
         header_t *const newHeader = (header_t *)( new char [sizeof( header_t ) + urlLen] );

         sequence = nextSequence_++ ;
         newHeader->size_      = initialSpace ;
         newHeader->sequence_  = sequence ;
         newHeader->completed_ = false ;
         newHeader->fd_        = -1 ;
         newHeader->data_      = 0 ;
         newHeader->temporary_ = false ;
         strcpy( newHeader->name_, url );
         
         //
         // add to head of list
         //
         list_add( &newHeader->chain_, &mru_ );

         //
         // add to hashes
         //
         cacheEntries_[newHeader->sequence_] = newHeader ;
         entriesByName_[url] = newHeader->sequence_ ;
         
         return true ;
      } // had or made space
      else
         fprintf( stderr, "Insufficient cache space for %s\n", url );
   }
   else
   {
      fprintf( stderr, "%s:%s Attempt to allocate existing file %s\n", __FILE__, __PRETTY_FUNCTION__, url );
      ABORT( 1 );
   }

   return false ;
}

bool ccDiskCache_t :: allocateMore
   ( unsigned sequence,              // input
     unsigned howMuchMore )          // input
{
   bySequence_t::const_iterator it = cacheEntries_.find( sequence );
   if( cacheEntries_.end() != it )
   {
      header_t &header = *((*it).second);
      if( !header.completed_ && ( 0 > header.fd_ ) && ( 0 == header.data_ ) )
      {
         unsigned long const freeSpace = maxSize_ - currentSize_ - reservedSize_ ;
         if( ( howMuchMore <= freeSpace ) || makeSpace( howMuchMore-freeSpace ) )
         {
            reservedSize_ += howMuchMore ;
   
            (*it).second->size_ += howMuchMore ;
            
            return true ;
         } // had or made space
         else
            fprintf( stderr, "Insufficient cache space to grow item %lu\n", sequence );
      }
      else
      {
         fprintf( stderr, "%s:%s Attempt to grow completed file %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
         ABORT( 1 );
      }
   }
   else
   {
      fprintf( stderr, "%s:%s Attempt to grow non-existent file %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
      ABORT( 1 );
   }

   return false ;
}

void ccDiskCache_t :: discardTemp( unsigned sequence )
{
   bySequence_t::iterator it = cacheEntries_.find( sequence );
   if( cacheEntries_.end() != it )
   {
      header_t &header = *((*it).second);
      if( !header.completed_ && ( 0 > header.fd_ ) && ( 0 == header.data_ ) )
      {
         list_del( &header.chain_ );
         cacheEntries_.erase( it );

         byName_t::iterator nameIt = entriesByName_.find( header.name_ );
         if( nameIt != entriesByName_.end() )
         {
            entriesByName_.erase( nameIt );
            if( reservedSize_ >= header.size_ )
            {
               reservedSize_ -= header.size_ ;
               delete [] (char *)&header ;
            }
            else
            {
               fprintf( stderr, "%s:%s:reserved space error for file %s\n", __FILE__, __PRETTY_FUNCTION__, header.name_ );
               ABORT(1);
            }
         }
         else
         {
            fprintf( stderr, "%s:%s:Cache consistency error for file %s\n", __FILE__, __PRETTY_FUNCTION__, header.name_ );
            ABORT(1);
         }
      }
      else
      {
         fprintf( stderr, "%s:%s:Attempt to discard completed file %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
         ABORT( 1 );
      }
   }
   else
   {
      fprintf( stderr, "%s:%s Attempt to discard non-existent file %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
      ABORT( 1 );
   }
}

bool ccDiskCache_t :: storeData
   ( unsigned    sequence,        // input : returned from allocateInitial
     unsigned    size,            // input : how much data to store. Must be less than allocation
     void const *data,            // input : data to write
     bool        temporary )
{
   bySequence_t::iterator it = cacheEntries_.find( sequence );
   if( cacheEntries_.end() != it )
   {
      header_t &header = *((*it).second);
      if( !header.completed_ && ( 0 > header.fd_ ) && ( 0 == header.data_ ) )
      {
         if( ( reservedSize_ >= header.size_ ) && ( size <= header.size_ ) )
         {
            std::string const cachedFileName = getCacheName( basePath_, sequence );
            int const fd = creat( cachedFileName.c_str(), S_IRWXU | S_IRGRP | S_IROTH );
            if( 0 < fd )
            {
               bool failed = false ;

               unsigned const nameLen = strlen( header.name_ ) + 1 ;
               int numWritten = write( fd, &nameLen, sizeof( nameLen ) );
               if( sizeof( nameLen ) == numWritten )
               {
                  numWritten = write( fd, header.name_, nameLen );
                  if( nameLen == numWritten )
                  {
                     unsigned pad = ( 4 - ( nameLen & 3 ) ) & 3 ;
                     if( pad )
                        write( fd, "\0\0\0\0", pad ); // pad to longword boundary

                     numWritten = write( fd, &size, sizeof( size ) );
                     if( sizeof( size ) == numWritten )
                     {
                        numWritten = write( fd, data, size );
                        if( size == (unsigned)numWritten )
                        {
                           // move to head of list
                           list_del( &header.chain_ );
                           list_add( &header.chain_, &mru_ );
                           reservedSize_ -= header.size_ ;
                           currentSize_  += header.size_ ;
                           header.completed_ = true ;
                           header.temporary_ = temporary ;
                        }
                        else
                        {
                           fprintf( stderr, "I/O error %m:%d out of %u bytes written for file %lu\n",
                                    numWritten, size, sequence );
                           failed = true ;
                        }
                     }
                     else
                     {
                        fprintf( stderr, "I/O error:%s\n", header.name_ );
                        failed = true ;
                     }
                  }
                  else
                  {
                     fprintf( stderr, "I/O error:%s\n", header.name_ );
                     failed = true ;
                  }
               }
               else
               {
                  fprintf( stderr, "I/O error:%s\n", header.name_ );
                  failed = true ;
               }

               close( fd );

               if( failed )
                  unlink( cachedFileName.c_str() );

               return !failed ;

            }
            else
            {
               perror( cachedFileName.c_str() );
               ABORT( 1 );
            }            
         }
         else
         {
            fprintf( stderr, "%s:%s:reserved space error for file %s\n", __FILE__, __PRETTY_FUNCTION__, header.name_ );
            ABORT(1);
         }
      }
      else
      {
         fprintf( stderr, "%s:%s:Attempt to store completed file %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
         ABORT( 1 );
      }
   }
   else
   {
      fprintf( stderr, "%s:%s Attempt to store non-existent file %lu\n", __FILE__, __PRETTY_FUNCTION__, sequence );
      ABORT( 1 );
   }
   return false ;
}

bool ccDiskCache_t :: makeSpace( unsigned long spaceNeeded )
{
   for( list_head *h = mru_.prev ; h != &mru_ ; )
   {
      header_t  &header = *((header_t *)h );
      list_head *prev = h->prev ;
      
      if( header.completed_ && ( 0 > header.fd_ ) && ( 0 == header.data_ ) )
      {
         unsigned long freed = header.size_ ;
         freeOne( header );
         if( spaceNeeded < freed )
            return true ; // done!
         else
            spaceNeeded -= freed ;
      }

      h = prev ;
   } // until wrapped 

   //
   // all items in use and not enough space
   //
   return false ;

}

void ccDiskCache_t :: freeOne( header_t &header )
{
   if( header.size_ <= currentSize_ )
   {
      byName_t :: iterator nameIt = entriesByName_.find( header.name_ );
      bySequence_t :: iterator seqIt = cacheEntries_.find( header.sequence_ );
      if( (nameIt != entriesByName_.end()) && (seqIt != cacheEntries_.end()) )
      {
         entriesByName_.erase( nameIt );
         cacheEntries_.erase( seqIt );
         list_del( &header.chain_ );
         currentSize_ -= header.size_ ;
         
         std::string const cachedFileName = getCacheName( basePath_, header.sequence_ );
         int const err = unlink( cachedFileName.c_str() );
         if( 0 == err )
         {
            delete [] (char *)( &header );
         }
         else
         {
            fprintf( stderr, "%s:%s:%m deleting %s, %s\n", __FILE__, __PRETTY_FUNCTION__, header.name_, cachedFileName.c_str() );
            ABORT( 1 );
         }
      }
      else
      {
         fprintf( stderr, "%s:%s cache index discrepancy for file %s\n", __FILE__, __PRETTY_FUNCTION__, header.name_ );
         ABORT( 1 );
      }
   }
   else
   {
      fprintf( stderr, "%s:%s size discrepancy for file %s\n", __FILE__, __PRETTY_FUNCTION__, header.name_ );
      ABORT( 1 );
   }
}

void ccDiskCache_t :: dump( void ) const 
{
   printf( "----> entries by name\n" );
   for( byName_t :: const_iterator nameIt = entriesByName_.begin(); nameIt != entriesByName_.end(); nameIt++ )
   {
      printf( "%08lx:%s\n", (*nameIt).second, (*nameIt).first.c_str() );
   }
   
   printf( "----> entries by sequence\n" );
   for( bySequence_t :: const_iterator seqIt = cacheEntries_.begin(); seqIt != cacheEntries_.end(); seqIt++ )
   {
      header_t const &header = *((*seqIt).second);
      printf( "%08lx:%d:%10lu:%s\n", (*seqIt).first, header.completed_, header.fd_, header.name_ );
   }

   printf( "----> entries by access time\n" );
   for( list_head *h = mru_.next ; h != &mru_ ; h = h->next )
   {
      header_t const &header = *((header_t const *) h );
      printf( "%08lx:%d:%10ld:%10lu:%s\n", header.sequence_, header.completed_, header.fd_, header.size_, header.name_ );
   }
}

   
ccDiskCache_t :: ccDiskCache_t
   ( char const    *dirName,
     unsigned long  maxSize )
   : basePath_( dirName ),
     maxSize_( maxSize ),
     currentSize_( 0 ),
     reservedSize_( 0 ),
     nextSequence_( 0 )
{
   if( 0 != basePath_.size() && ( '/' != basePath_[basePath_.size()-1] ) )
      basePath_ += '/' ;
   
   INIT_LIST_HEAD(&mru_);
   DIR *dh = opendir( basePath_.c_str() );
   if( 0 == dh )
   {
      int result = mkdir( basePath_.c_str(), 0777 );
      if( 0 == result )
      {
         dh = opendir( basePath_.c_str() );
      }
      else
         perror( basePath_.c_str() );
   }

   if( dh )
   {
      std::string fullName ;
      fullName.reserve( 256 );
      unsigned const expectedSize = basePath_.size() + 8 ;

      unsigned count = 0 ;
      struct dirent entry, *pEntry ;
      while( ( 0 == readdir_r( dh, &entry, &pEntry ) )
             &&
             ( 0 != pEntry ) )
      {
         if( DT_REG == entry.d_type )
         {
            fullName = basePath_ ;
            fullName += entry.d_name ;
            if( expectedSize == fullName.size() )
            {
               char *cEnd ;
               unsigned long const sequence = strtoul( entry.d_name, &cEnd, 16 );
               if( '\0' == *cEnd )
               {
                  if( sequence >= nextSequence_ )
                     nextSequence_ = sequence + 1 ;

                  memFile_t fIn( fullName.c_str() );
                  if( fIn.worked() )
                  {
                     if( 9 < fIn.getLength() ) // namelen, datalen, one byte of name, one data?
                     {
                        unsigned long urlLen ;
                        memcpy( &urlLen, fIn.getData(), sizeof( urlLen ) );
                        unsigned const pad = ( 4 - ( urlLen & 3 ) ) & 3 ;

                        if( urlLen <= fIn.getLength() - sizeof( urlLen ) - sizeof( unsigned long ) - 1 - pad ) // min 1 byte of data, 4 bytes of length
                        {
                           char const *url = (char const *)fIn.getData() + sizeof( urlLen );
                           if( '\0' == url[urlLen-1] ) // urlLen includes NULL terminator
                           {
                              unsigned long dataSize ;
                              memcpy( &dataSize, url + urlLen + pad, sizeof( dataSize ) );
                              if( fIn.getLength() - 8 - pad == urlLen + dataSize )
                              {
                                 unsigned long const freeSpace = maxSize_ - currentSize_ - reservedSize_ ;
                                 if( ( dataSize < freeSpace ) || makeSpace( dataSize-freeSpace ) )
                                 {
                                    header_t *newHeader = (header_t *)( new char [sizeof( header_t ) + urlLen] );
                                    newHeader->size_      = dataSize ;
                                    newHeader->sequence_  = sequence ;
                                    newHeader->completed_ = true ;
                                    newHeader->fd_        = -1 ;
                                    newHeader->data_      = 0 ;
                                    newHeader->temporary_ = false ;
                                    memcpy( newHeader->name_, url, urlLen );
                                    newHeader->name_[urlLen] = '\0' ;
                                    count++ ;
   
                                    //
                                    // add to head of list
                                    //
                                    list_add( &newHeader->chain_, &mru_ );
   
                                    //
                                    // add to hashes
                                    //
                                    cacheEntries_[newHeader->sequence_] = newHeader ;
                                    entriesByName_[newHeader->name_] = newHeader->sequence_ ;
                                    currentSize_ += dataSize ;
                                    continue;
                                 }
                                 else
                                 {
                                    fprintf( stderr, "Error allocating space\n" );
                                    ABORT( 1 );
                                 }
                              } // file has reasonable sizes
                              else
                                 fprintf( stderr, "%s:%s: invalid name or data size:%lu/%lu/%lu\n", fullName.c_str(), url, urlLen, dataSize, fIn.getLength() );
                           }
                           else
                              fprintf( stderr, "Invalid terminator in url:%s\n", fullName.c_str() );
                        }
                        else
                           fprintf( stderr, "%s:invalid url size:%lu/%lu\n", fullName.c_str(), urlLen, fIn.getLength() );
                     } // enough room for namesize, 1 byte url, filesize, 1 byte content
                     else
                        fprintf( stderr, "%s:file too small\n", fullName.c_str() );
                  }
                  else
                     fprintf( stderr, "%s:%m", fullName.c_str() );
               }
               else
                  fprintf( stderr, "unknown file %s:%s\n", fullName.c_str(), cEnd );
            } // looks like one of ours
            else
               fprintf( stderr, "unknown file %s\n", fullName.c_str() );
            
            unlink( fullName.c_str() );
         }
      }

      closedir( dh );

   }
   else
      fprintf( stderr, "Error %m opening or creating directory %s\n", basePath_.c_str() );
}

ccDiskCache_t :: ~ccDiskCache_t( void )
{
}

void ccDiskCache_t :: retrieveURLs( std::vector<std::string> &urls ) const 
{
   for( list_head *h = mru_.next ; h != &mru_ ; h = h->next )
   {
      header_t const &header = *((header_t const *) h );
      urls.push_back( header.name_ );
   }
}

static ccDiskCache_t *diskCache = 0 ;

ccDiskCache_t &getDiskCache( void )
{
   if( 0 == diskCache )
   {
      char *path = getenv( "CURLTMPDIR" );
      if( 0 == path )
         path = "/tmp/curl" ;
      
      unsigned long maxSize = 3*(1<<20);
      char *cSize = getenv( "CURLTMPSIZE" );
      if( cSize )
         maxSize = strtoul( cSize, 0, 0 );

      diskCache = new ccDiskCache_t( path, maxSize );
   }

   return *diskCache ;
}

void shutdownCCDiskCache( void )
{
   if( 0 != diskCache )
   {
      delete diskCache ;
      diskCache = 0 ;
   }
}

#ifdef __STANDALONE__

int main( void )
{
   try {
      //
      // attempt to trace all parts of the code
      //
      {
         printf( "---> existing cache entries\n" );
         ccDiskCache_t &cache = getDiskCache();
         std::vector<std::string> urls ;
         cache.retrieveURLs( urls );
         for( unsigned i = 0 ; i < urls.size(); i++ )
         {
            printf( "%s\n", urls[i].c_str() );
            if( !cache.deleteFromCache( urls[i].c_str() ) )
               fprintf( stderr, "Error deleting %s from cache\n", urls[i].c_str() );
         }
         shutdownCCDiskCache();
      }
   
      //
      // try shutting down with no allocation
      //
      shutdownCCDiskCache();
      
      //
      // now walk current directory, starting to add files, but cancelling
      //
      {
         ccDiskCache_t &cache = getDiskCache();
         DIR *dh = opendir( "./" );
         if( 0 != dh )
         {
            unsigned numRead = 0 ;
            struct dirent entry, *pEntry ;
            while( ( 0 == readdir_r( dh, &entry, &pEntry ) )
                   &&
                   ( 0 != pEntry ) )
            {
               if( DT_REG == entry.d_type ) 
               {
                  std::string url( "file://./" );
                  url += entry.d_name ;
                  unsigned sequence ;
                  if( !cache.find( url.c_str(), sequence ) )
                  {
                     if( cache.allocateInitial( url.c_str(), 1000, sequence ) )
                     {
                        if( cache.allocateMore( sequence, 100 ) )
                        {
                           cache.discardTemp( sequence );
                           numRead++ ;
                        }
                        else
                           fprintf( stderr, "Error growing file %s\n", url.c_str() );
                     }
                     else
                        fprintf( stderr, "Error allocating space for %s\n", url.c_str() );
                  }
                  else
                     fprintf( stderr, "?? found new file %s\n", url.c_str() );
               }
            }

            printf( "found (didn't find) %u entries\n", numRead );
            closedir( dh );
         }
      }
      
      //
      // walk current directory, adding each file from current directory in two parts
      //
      {
         ccDiskCache_t &cache = getDiskCache();
         DIR *dh = opendir( "./" );
         if( 0 != dh )
         {
            unsigned numRead = 0 ;
            struct dirent entry, *pEntry ;
            while( ( 0 == readdir_r( dh, &entry, &pEntry ) )
                   &&
                   ( 0 != pEntry ) )
            {
               if( DT_REG == entry.d_type ) 
               {
                  std::string url( "file://./" );
                  url += entry.d_name ;
                  unsigned sequence ;
                  if( !cache.find( url.c_str(), sequence ) )
                  {
                     memFile_t fIn( entry.d_name );
                     if( fIn.worked() && ( 0 < fIn.getLength() ) )
                     {
                        if( cache.allocateInitial( url.c_str(), fIn.getLength()/2, sequence ) )
                        {
                           if( cache.allocateMore( sequence, fIn.getLength()-(fIn.getLength()/2) ) )
                           {
                              cache.storeData( sequence, fIn.getLength(), fIn.getData(), false );
                              numRead++ ;
                           }
                           else
                              fprintf( stderr, "Error growing file %s\n", url.c_str() );
                        }
                        else
                           fprintf( stderr, "Error allocating space for %s\n", url.c_str() );
                     }
                  }
                  else
                     fprintf( stderr, "?? found new file %s\n", url.c_str() );
               }
            }

            printf( "found (and added) %u entries\n", numRead );
            closedir( dh );

            dh = opendir( "./" );
            if( 0 != dh )
            {
               unsigned numLeft = 0 ;
               struct dirent entry, *pEntry ;
               while( ( 0 == readdir_r( dh, &entry, &pEntry ) )
                      &&
                      ( 0 != pEntry ) )
               {
                  if( DT_REG == entry.d_type ) 
                  {
                     std::string url( "file://./" );
                     url += entry.d_name ;
                     unsigned sequence ;
                     if( cache.find( url.c_str(), sequence ) )
                     {
                        memFile_t fIn( entry.d_name );
                        if( fIn.worked() && ( 0 < fIn.getLength() ) )
                        {
                           void const *data ;
                           unsigned long cacheLen ;
                           cache.inUse( sequence, data, cacheLen );
                              
                           if( fIn.getLength() == cacheLen )
                           {
                              if( 0 == memcmp( fIn.getData(), data, fIn.getLength() ) )
                              {
                                 numLeft++ ;
                                 cache.notInUse( sequence );
                              }
                              else
                                 fprintf( stderr, "cache content mismatch for entry %lu:%s\n", sequence, entry.d_name );
                           }
                           else
                              fprintf( stderr, "cache size mismatch %lu:%lu for entry %lu:%s\n", fIn.getLength(), cacheLen, sequence, entry.d_name );
                        }
                     } // entry is still in cache
                  }
               }
   
               printf( "%u entries remain \n", numLeft );
               closedir( dh );
            }
         }
      } // limit scope of cache
      
      //
      // delete 'em all
      //
      {
         ccDiskCache_t &cache = getDiskCache();
         std::vector<std::string> urls ;
         cache.retrieveURLs( urls );
         printf( "%u entries remaining in cache\n", urls.size() );
         for( unsigned i = 0 ; i < urls.size(); i++ )
         {
            if( !cache.deleteFromCache( urls[i].c_str() ) )
               fprintf( stderr, "Error deleting %s from cache\n", urls[i].c_str() );
         }
         shutdownCCDiskCache();
      }

      //
      // verify deletion
      //
      {
         ccDiskCache_t &cache = getDiskCache();
         std::vector<std::string> urls ;
         cache.retrieveURLs( urls );
         printf( "%lu urls remaining\n", urls.size() );
         shutdownCCDiskCache();
      }

   }
   catch( abort_t abort ) {
      fprintf( stderr, "exited with status %lu\n", abort.status_ );
   };
   
   return 0 ;
}

#endif
