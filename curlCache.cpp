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
 * Revision 1.6  2002-10-24 13:18:25  ericn
 * -modified for relative URLs
 *
 * Revision 1.5  2002/10/15 05:00:55  ericn
 * -.
 *
 * Revision 1.4  2002/10/13 14:36:54  ericn
 * -made cache usage optional
 *
 * Revision 1.3  2002/10/13 13:42:13  ericn
 * -got rid of content reference for file posts
 *
 * Revision 1.2  2002/10/09 01:10:07  ericn
 * -added post support
 *
 * Revision 1.1.1.1  2002/09/28 16:50:46  ericn
 * -Initial import
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
#include <zlib.h>
#include "jsURL.h"

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
            data_     = ((char const *)mem ) + sizeof( curlFile_t::header_t );
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
curlFile_t curlCache_t :: get( char const url[], bool useCache )
{
   std::string absolute ;
   std::string cacheName ;

   if( absoluteURL( url, absolute ) )
   {
      cacheName = getCachedName( absolute.c_str() );
      struct stat st ;
      int const statResult = stat( cacheName.c_str(), &st );
      if( useCache && ( 0 == statResult ) )
      {
      } // file in cache... return it
      else
      {
         if( 0 == statResult )
            unlink( cacheName.c_str() ); // get rid of old one
   
         makeSpace( 1, 1<<20 ); // 1MB
   
         CURL *curl ;
         int   fd ;
         if( startTransfer( cacheName, absolute.c_str(), curl, fd ) )
         {
            CURLcode result = curl_easy_perform( curl );
            if( 0 == result )
            {
               if( store( curl, fd ) )
               {
               }
               else
                  unlink( cacheName.c_str() );
            }
            else
               discard( cacheName, curl, fd );
         }
   
      } // file not found, retrieve it
   }
   
   return curlFile_t( cacheName.c_str() );
}


curlFile_t curlCache_t :: post( curlRequest_t const &req, bool useCache )
{
   std::string const cacheName( getCachedName( req ) );

   struct stat st ;
   int const statResult = stat( cacheName.c_str(), &st );
   if( useCache && ( 0 == statResult ) )
   {
   } // file in cache... return it
   else
   {
      if( 0 == statResult )
         unlink( cacheName.c_str() );

      makeSpace( 1, 1<<20 ); // 1MB

      CURL *curl ;
      int   fd ;
      struct curl_slist  *headerlist ;
      struct HttpPost    *postHead ;
      
      if( startPost( cacheName, req, curl, fd, headerlist, postHead ) )
      {
         CURLcode result = curl_easy_perform( curl );
         if( 0 == result )
         {
            if( store( curl, fd ) )
            {
            }
            else
               unlink( cacheName.c_str() );
         }
         else
            discard( cacheName, curl, fd );

         curl_formfree( postHead );
         curl_slist_free_all( headerlist );
      }

   } // file not found, retrieve it
   
   return curlFile_t( cacheName.c_str() );
}

curlRequest_t :: curlRequest_t( char const url[] )
   : url_( "" ),
     hasFile_( false )
{
   std::string absolute ;

   if( absoluteURL( url, absolute ) )
   {
      url_ = absolute ;
   }
}
   
curlRequest_t :: ~curlRequest_t( void )
{
}

void curlRequest_t :: addVariable
   ( char const *name,
     char const *value )
{
   param_t param ;
   param.name_               = name ;
   param.isFile_             = false ;
   param.value_.stringValue_ = value ;
   parameters_.push_back( param );
}

void curlRequest_t :: addFile
   ( char const   *name,
     char const   *path )
{
   param_t param ;
   param.name_             = name ;
   param.isFile_           = true ;
   param.value_.fileValue_ = path ;
   parameters_.push_back( param );
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
   ( std::string const &cachedName,
     char const         targetURL[],
     CURL             *&cHandle,
     int               &fd )
{
   fd = creat( cachedName.c_str(), S_IRWXU | S_IRGRP | S_IROTH );
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
            curl_easy_cleanup( cHandle );
            cHandle = 0 ;
         }
      }
      
      close( fd );
      fd = -1 ;
   }

   return false ;
}

bool curlCache_t :: startPost
   ( std::string const   &cachedName,
     curlRequest_t const &req,
     CURL               *&cHandle,
     int                 &fd,
     struct curl_slist  *&headerlist,
     struct HttpPost    *&postHead )
{
   fd = creat( cachedName.c_str(), S_IRWXU | S_IRGRP | S_IROTH );
   if( 0 < fd )
   {
      curlFile_t::header_t header ;
      memset( &header, 0, sizeof( header ) );
      if( sizeof( header ) == write( fd, &header, sizeof( header ) ) )
      {
         cHandle = curl_easy_init();
         if( 0 != cHandle )
         {
            CURLcode result = curl_easy_setopt( cHandle, CURLOPT_URL, req.getURL() );
   
            if( 0 == result )
            {
               postHead = NULL;
               struct HttpPost* last = NULL;
               for( unsigned i = 0 ; i < req.parameters_.size(); i++ )
               {
                  curlRequest_t::param_t const &param = req.parameters_[i];

                  if( !param.isFile_ )
                     curl_formadd( &postHead, &last, 
                                   CURLFORM_PTRNAME, param.name_,
                                   CURLFORM_PTRCONTENTS, param.value_.stringValue_,
                                   CURLFORM_END );
                  else
                     curl_formadd( &postHead, &last,
                                   CURLFORM_PTRNAME, param.name_,
                                   CURLFORM_FILE, param.value_.fileValue_,
                                   CURLFORM_CONTENTTYPE, "application/octet-stream",
                                   CURLFORM_END );
               }
               
               headerlist = curl_slist_append( NULL, "Expect:" );
               curl_easy_setopt( cHandle, CURLOPT_HTTPHEADER, headerlist );

               result = curl_easy_setopt( cHandle, CURLOPT_HTTPPOST, postHead );
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
                           return true ;
                        }
                     }
                  }
               }

               curl_formfree( postHead );
               postHead = 0 ;
               curl_slist_free_all( headerlist );
               headerlist = 0 ;

            }

            curl_easy_cleanup( cHandle );
            cHandle = 0 ;
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
   ( CURL      *curl,   // handle to connection
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
   ( std::string const &cacheName,
     CURL              *curl,
     int               fd )
{
   curl_easy_cleanup( curl );
   close( fd );
   unlink( cacheName.c_str() );
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

std::string curlCache_t :: getCachedName( curlRequest_t const &req )
{
   std::string name( getCachedName( req.url_.c_str() ) );
   unsigned long adler = adler32( 0, (Bytef const *)name.c_str(), name.size() );
   for( unsigned i = 0 ; i < req.parameters_.size(); i++ )
   {
      curlRequest_t::param_t const &param = req.parameters_[i];
      adler = adler32( adler, (Bytef const *)param.name_, strlen( param.name_ ) );
      if( !param.isFile_ )
      {
         adler = adler32( adler, (Bytef const *)param.value_.stringValue_, strlen( param.value_.stringValue_ ) );
      } // string value
      else
      {
         adler = ~adler ;
         adler = adler32( adler, (Bytef const *)param.value_.fileValue_, strlen( param.value_.fileValue_ ) );
      } // file
   }
   
   char adlerHex[10];
   sprintf( adlerHex, "^%08lX", adler );
   name += adlerHex ;
   
   return name ;
}

static curlCache_t *cache_ = 0 ;

curlCache_t &getCurlCache( void )
{
   if( 0 == cache_ )
      cache_ = new curlCache_t( "/tmp/curl", 8*(1<<20), 5 );

   return *cache_ ;
}

#ifdef STANDALONE 
#include "memFile.h"

static void printInfo( curlFile_t const &f )
{
   printf( "effective url %s\n", f.getEffectiveURL() );
   printf( "http code %ld\n", f.getHttpCode() );
   time_t ft = (time_t)f.getFileTime();
   tm const *ftm = localtime( &ft );
   printf( "file time %s", asctime( ftm ) );
   printf( "mime type %s\n", f.getMimeType() );
   if( 0 != strstr( f.getMimeType(), "text" ) )
   {
      printf( "content:<" );
      fwrite( f.getData(), f.getSize(), 1, stdout );
      printf( ">\n" );
   }
}

static char const usage[] = {
   "Usage : curlCache url [var value]|[var @file]...\n" 
};

int main( int argc, char const * const argv[] )
{
   if( 1 < argc )
   {
      char const *url = argv[1];
      curlCache_t &cache = getCurlCache();
      if( 2 == argc )
      {
         curlFile_t f( cache.get( url ) );
         if( f.isOpen() )
         {
            printf( "%s found in cache, %lu bytes\n", url, f.getSize() );
            printInfo( f );
            curlFile_t f2( f );
            printInfo( f2 );
         }
         else
            printf( "%s not found in cache\n", url );
      }
      else if( 0 == ( argc & 1 ) )
      {
         std::vector<memFile_t> files ;
         curlRequest_t req( url );

         for( int arg = 2 ; arg < argc ; arg += 2 )
         {
            if( '@' != argv[arg+1][0] )
            {
               req.addVariable( argv[arg], argv[arg+1] );
            } // string parameter
            else
            {
               req.addFile( argv[arg], argv[arg+1]+1 );
            } // file parameter
         }

         //
         // okay, now post or get the file
         //
         printf( "cacheName == %s\n", cache.getCachedName( req ).c_str() );
         curlFile_t f( cache.post( req ) );
         if( f.isOpen() )
         {
            printf( "%s found in cache, %lu bytes\n", url, f.getSize() );
            printInfo( f );
            curlFile_t f2( f );
            printInfo( f2 );
         }
         else
            printf( "%s not found in cache\n", url );
      }
      else
      {
         fprintf( stderr, usage );
         fprintf( stderr, "either one or an odd number of parameters\n" );
      }
   }
   else
      fprintf( stderr, usage );

   return 0 ;
}

#endif
