/*
 * Module curlCache.cpp
 *
 * This module defines the curlFile_t and curlCache_t 
 * classes.
 *
 *
 * Change History : 
 *
 * $Log: curlCache.cpp,v $
 * Revision 1.1  2002-09-28 16:50:46  ericn
 * Initial revision
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "curlCache.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include "dirByATime.h"

bool findEnd( void const   *mem,            // input : mapped file
              unsigned long fileSize,       // input : size of mapped memory
              unsigned long dataSize,       // input : size of content portion
              char const  *&effectiveURL,   // output : effective url
              char const  *&mimeType )      // output : mime type

{
   effectiveURL = 0 ;
   mimeType     = 0 ;

   char const *eof = (char const *)mem + fileSize - 1 ;
   if( '\0' == *eof )
   {
      char const *endOfContent = (char const *)mem + sizeof( curlFile_t::header_t ) + dataSize ;

      --eof ; 
      while( eof > endOfContent )
      {
         if( '\0' != *eof )
            eof-- ;
         else
            break;
      }
      
      if( eof > endOfContent )
      {
         mimeType = eof+1 ;
         --eof ;
         while( eof > endOfContent )
         {
            if( '\0' != *eof )
               eof-- ;
            else
               break;
         }
         
         if( eof == endOfContent )
         {
            effectiveURL = eof ;
            return true ;
         } // perfect! 
//         else
//            fprintf( stderr, "early termination of URL\n" );
      } // room for effective URL
//      else
//         fprintf( stderr, "no room for effective URL\n" );

   } // have trailing NULL, walk backwards to start of mime type
//   else
//      fprintf( stderr, "no trailing NULL\n" );

   return false ;
}

curlFile_t :: curlFile_t( char const *tmpFileName )
 : fd_( open( tmpFileName, O_RDONLY ) ),
   fileSize_( 0 ),
   size_( 0 ),
   data_( 0 ),
   effective_( 0 ),
   httpCode_( -1L ),
   fileTime_( -1L ),
   mimeType_( 0 )
{
   if( 0 <= fd_ )
   {
      //
      // turkey carving :
      //
      // Here we parse the actual file:
      //
      // Basically, this is fixed-length stuff, followed by http
      // content, followed by variable-length strings
      // 
      //    bytes                   content
      //    0-3                     length of real content
      //    4-7                     httpCode_
      //    8-11                    fileTime_ ;
      //
      //    12..contentLength+11    real http content
      //    
      //    ...                     effective_ (NULL-terminated)
      //    ...                     mimeType_  (NULL-termineted)
      //
      off_t const fileSize = lseek( fd_, 0, SEEK_END );
      if( sizeof( curlFile_t::header_t ) + 2 < fileSize )
      {
         lseek( fd_, 0, SEEK_SET );
         curlFile_t::header_t header ;
         if( sizeof( header ) == read( fd_, &header, sizeof( header ) ) )
         {
            if( header.size_ + sizeof( header ) + 2 <= fileSize )
            {
               void *mem = mmap( 0, fileSize, PROT_READ, MAP_PRIVATE, fd_, 0 );
               if( MAP_FAILED != mem )
               {
                  if( findEnd( mem, fileSize, header.size_, effective_, mimeType_ ) )
                  {
                     fileSize_     = fileSize ;
                     size_         = header.size_ ;
                     data_         = ((char const *)mem ) + sizeof( header );
                     httpCode_     = header.httpCode_ ;
                     fileTime_     = header.fileTime_ ;
                     return ;
                  }

                  munmap( mem, fileSize );

               } // mapped file
//               else
//                  fprintf( stderr, "error mapping %s\n", tmpFileName );
            } // room for content and trailers
//            else
//               fprintf( stderr, "no room for content in %s\n", tmpFileName );
         } // read file header
//         else
//            fprintf( stderr, "error reading file header in %s\n", tmpFileName );
      } // room for file header and NULL terminators
//      else
//         fprintf( stderr, "file %s too small\n", tmpFileName );

      close( fd_ );
      fd_ = -1 ; // failed. make sure we don't say isOpen
   }
   else
      perror( tmpFileName );
}

curlFile_t :: curlFile_t( curlFile_t const &rhs )
 : fd_( dup( rhs.fd_ ) ),
   fileSize_( 0 ),
   size_( rhs.size_ ),
   data_( 0 ),
   effective_( 0 ),
   httpCode_( -1L ),
   fileTime_( -1L ),
   mimeType_( 0 )
{
   if( 0 <= fd_ )
   {
      void *mem = mmap( 0, rhs.fileSize_, PROT_READ, MAP_PRIVATE, fd_, 0 );
      if( mem )
      {
         if( findEnd( mem, rhs.fileSize_, rhs.size_, effective_, mimeType_ ) )
         {
            fileSize_ = rhs.fileSize_ ;
            size_     = rhs.size_ ;
            data_     = mem ;
            httpCode_ = rhs.httpCode_ ;
            fileTime_ = rhs.fileTime_ ;
            return ;
         }
         munmap( mem, rhs.fileSize_ );
      }
      close( fd_ );
   }
}

curlFile_t :: ~curlFile_t( void )
{
   if( ( 0 != data_ ) && ( 0 != fileSize_ ) )
      munmap( ( (char *)data_ ) - sizeof( curlFile_t::header_t ), fileSize_ );
   if( 0 < fd_ )
      close( fd_ );
}



//
// returns open curlFile_t if found in cache
//
curlFile_t curlCache_t :: get( char const url[] )
{
   std::string const cacheName( getCachedName( url ) );
   struct stat st ;
   if( 0 == stat( cacheName.c_str(), &st ) )
   {
   } // file in cache... return it
   else
   {
      makeSpace( 1, 1<<20 ); // 1MB

      CURL *curl ;
      int   fd ;
      if( startTransfer( url, curl, fd ) )
      {
         CURLcode result = curl_easy_perform( curl );
         if( 0 == result )
         {
            if( store( url, curl, fd ) )
            {
            }
            else
               unlink( cacheName.c_str() );
         }
         else
            discard( url, curl, fd );
      }

   } // file not found, retrieve it
   
   return curlFile_t( cacheName.c_str() );
}


static size_t writeData( void *buffer, size_t size, size_t nmemb, void *userp )
{
   int const fd = (int)userp ;
   return write( fd, buffer, size*nmemb );
}
 
void curlCache_t :: makeSpace( unsigned numFiles, unsigned long bytes )
{
   dirByATime_t dir( dirName_ );
   unsigned filesToKill ;
   if( maxEntries_ > dir.numEntries() + numFiles )
   {
      filesToKill = 0 ;
   }
   else
   {
      filesToKill = dir.numEntries() + numFiles - maxEntries_ ;
   }

   unsigned long bytesToKill ;
   if( maxSize_ > dir.totalSize() + bytes )
   {
      bytesToKill = 0 ;
   }
   else
   {
      bytesToKill = dir.totalSize() + bytes - maxSize_ ;
   }

   unsigned idx = 0 ;
   while( ( filesToKill > 0 ) || ( bytesToKill > 0 ) )
   {
      dirByATime_t::details_t const entry = dir.getEntry( idx++ );
      unlink( entry.dirent_.d_name );
      if( filesToKill > 0 )
         --filesToKill ;
      if( bytesToKill > 0 )
      {
         if( bytesToKill > entry.stat_.st_size )
            bytesToKill -= entry.stat_.st_size ;
         else
            bytesToKill = 0 ;
      }
   }
}

//
// use this function to start a transfer
//
bool curlCache_t :: startTransfer
   ( char const targetURL[],
     CURL     *&cHandle,
     int       &fd )
{
   std::string tmpName( getCachedName( targetURL ) );
   fd = creat( tmpName.c_str(), S_IRWXU | S_IRGRP | S_IROTH );
   if( 0 < fd )
   {
      curlFile_t::header_t header ;
      memset( &header, 0, sizeof( header ) );
      if( sizeof( header ) == write( fd, &header, sizeof( header ) ) )
      {
         cHandle = curl_easy_init();
         if( 0 != cHandle )
         {
            CURLcode result = curl_easy_setopt( cHandle, CURLOPT_URL, targetURL );
   
            if( 0 == result )
            {
               result = curl_easy_setopt( cHandle, CURLOPT_WRITEFUNCTION, writeData );
               if( 0 == result )
               {
                  result = curl_easy_setopt( cHandle, CURLOPT_WRITEDATA, fd );
                  if( 0 == result )
                  {
                     result = curl_easy_setopt( cHandle, CURLOPT_FILETIME, &curlTimeBuffer_ );
                     if( 0 == result )
                     {
                        result = curl_easy_setopt( cHandle, CURLOPT_FILETIME, &curlTimeBuffer_ );
                        if( 0 == result )
                        {
                           return true ;
                        }
                     }
                  }
               }
            }
         }
      }
      
      close( fd );
      fd = -1 ;
   }

   return false ;
}


//
// use this function to complete a transfer after curl_easy_perform()
//
bool curlCache_t :: store
   ( char const targetURL[],
     CURL      *curl,   // handle to connection
     int        fd )
{
   bool worked = false ;
   off_t const fileSize = lseek( fd, 0, SEEK_END );
   if( fileSize >= sizeof( curlFile_t::header_t ) )
   {
      off_t const dataSize = fileSize - sizeof( curlFile_t::header_t );

      curlFile_t::header_t header ;
      CURLcode result = curl_easy_getinfo( curl, CURLINFO_HTTP_CODE, &header.httpCode_ );
      if( 0 == result )
      {
         result = curl_easy_getinfo( curl, CURLINFO_FILETIME, &header.fileTime_ );
         if( 0 == result )
         {
            char const *effURL ;
            result = curl_easy_getinfo( curl, CURLINFO_EFFECTIVE_URL, &effURL );
            if( 0 == result )
            {
               char const *mime ;
               result = curl_easy_getinfo( curl, CURLINFO_CONTENT_TYPE, &mime );
               if( 0 == result )
               {
                  unsigned const eLen = strlen( effURL ) + 1 ;
                  int numWritten = write( fd, effURL, eLen );
                  if( eLen == (unsigned)numWritten )
                  {
                     unsigned const mLen = strlen( mime ) + 1 ;
                     numWritten = write( fd, mime, mLen );
                     if( mLen == (unsigned)numWritten )
                     {
                        lseek( fd, 0, SEEK_SET );
                        header.size_ = dataSize ;
                        numWritten = write( fd, &header, sizeof( header ) );
                        if( sizeof( header ) == (unsigned)numWritten )
                        {
                           worked = true ;
                        }
                        else
                           fprintf( stderr, "Error %m/%d writing file header\n", numWritten );
                     }
                     else
                        fprintf( stderr, "Error %m/%d writing mimeType\n", numWritten );
                  }
                  else
                     fprintf( stderr, "Error %m/%d writing effective URL\n", numWritten );
               }
               else
                  fprintf( stderr, "Error %u getting content type\n", result );
            }
            else
               fprintf( stderr, "Error %u getting effective URL\n", result );
         }
         else
            fprintf( stderr, "Error %u getting file time\n", result );
      }
      else
         fprintf( stderr, "Error %u getting http code\n", result );
   }
   else
      fprintf( stderr, "file too small %lu\n", fileSize );

   curl_easy_cleanup( curl );
   close( fd );
   return worked ;
}


//
// use this function to discard a transfer
//
void curlCache_t :: discard
   ( char const targetURL[],
     CURL      *curl,
     int        fd )
{
   curl_easy_cleanup( curl );
   close( fd );
   std::string tmpName( getCachedName( targetURL ) );
   unlink( tmpName.c_str() );
}


curlCache_t :: curlCache_t
   ( char const    *dirName,
     unsigned long  maxSize,
     unsigned long  maxEntries )
   : dirName_( strdup( dirName ) ),
     maxSize_( maxSize ),
     maxEntries_( maxEntries )
{
}
   
curlCache_t :: ~curlCache_t( void )
{
}

std::string curlCache_t :: getCachedName( char const url[] )
{
   std::string path( dirName_ );
   path += '/' ;
   char *escaped = curl_escape( url, strlen( url ) );
   path += escaped ;
   free( escaped );
   return path ;
}

static curlCache_t *cache_ = 0 ;

curlCache_t &getCurlCache( void )
{
   if( 0 == cache_ )
   {
      cache_ = new curlCache_t( "/tmp/curl", 8*(1<<20), 5 );
   }

   return *cache_ ;
}

#ifdef STANDALONE 
#include <stdio.h>

static void printInfo( curlFile_t const &f )
{
   printf( "effective url %s\n", f.getEffectiveURL() );
   printf( "http code %ld\n", f.getHttpCode() );
   time_t ft = (time_t)f.getFileTime();
   tm const *ftm = localtime( &ft );
   printf( "file time %s", asctime( ftm ) );
   printf( "mime type %s\n", f.getMimeType() );
}

int main( int argc, char const * const argv[] )
{
   if( 1 < argc )
   {
      curlCache_t &cache = getCurlCache();
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         curlFile_t f( cache.get( argv[arg] ) );
         if( f.isOpen() )
         {
            printf( "%s found in cache, %lu bytes\n", argv[arg], f.getSize() );
            printInfo( f );
            curlFile_t f2( f );
            printInfo( f2 );
         }
         else
            printf( "%s not found in cache\n", argv[arg] );
      }
   }
   else
      fprintf( stderr, "Usage : curlCache url [url...]\n" );

   return 0 ;
}

#endif
