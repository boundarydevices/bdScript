/*
 * Module urlFile.cpp
 *
 * This module defines the methods of the urlFile_t 
 * class as declared in urlFile.h
 *
 *
 * Change History : 
 *
 * $Log: urlFile.cpp,v $
 * Revision 1.1  2002-09-28 16:50:46  ericn
 * Initial revision
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "urlFile.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include "curlCache.h"

urlFile_t :: urlFile_t( char const url[] )
   : fd_( -1 ),
     size_( 0 ),
     data_( 0 ),
     mappedSize_( 0 ),
     mapPtr_( 0 )
{
   char const *const protoEnd = strchr( url, ':' );
   if( protoEnd )
   {
      unsigned protoL = protoEnd-url ;
      if( ( 4 == protoL ) && ( 0 == strncasecmp( "file", url, protoL ) ) )
      {
         fd_ = open( url+7, O_RDONLY );
         if( 0 <= fd_ )
         {
            int eof = lseek( fd_, 0, SEEK_END );
            if( 0 <= eof )
            {
               mappedSize_ = size_ = (unsigned)eof ;
               mapPtr_ = mmap( 0, size_, PROT_READ, MAP_PRIVATE, fd_, 0 );
               if( mapPtr_ )
               {
                  data_ = mapPtr_ ;
                  return ;
               } // worked
            }

            close( fd_ );
            fd_ = -1 ;
         }
      } // local
      else if( ( ( 4 == protoL ) && ( 0 == strncasecmp( "http", url, protoL ) ) )
               ||
               ( ( 5 == protoL ) && ( 0 == strncasecmp( "https", url, protoL ) ) ) )
      {
         curlFile_t f( getCurlCache().get( url ) );
         if( f.isOpen() )
         {
            fd_ = dup( f.fd_ );
            if( 0 <= fd_ )
            {
               mapPtr_ = mmap( 0, f.fileSize_, PROT_READ, MAP_PRIVATE, fd_, 0 );
               if( mapPtr_ )
               {
                  size_ = f.getSize();
                  mappedSize_ = f.fileSize_ ;
                  data_ = (char *)mapPtr_ + sizeof( curlFile_t::header_t );
                  return ;
               }
            }
            
            close( fd_ );
         
         }
      } // web url 
   }
}

urlFile_t :: ~urlFile_t( void )
{
   if( 0 != mapPtr_ )
      munmap( (void *)mapPtr_, mappedSize_ );

   if( 0 <= fd_ )
      close( fd_ );
}


#ifdef STANDALONE
#include <stdio.h>
#include "zlib.h"

int main( int argc, char const * const argv[] )
{
   if( 1 < argc ) 
   {
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         urlFile_t f( argv[arg] );
         if( f.isOpen() )
         {
            unsigned a = adler32( 0, (unsigned char *)f.getData(), f.getSize() );
            printf( "%08x - %s\n", a, argv[arg] );
         }
         else
            fprintf( stderr, "Error %m opening %s\n", argv[arg] );
      }
   }
   else
      fprintf( stderr, "Usage : %s url [url...]\n", argv[0] );

   return 0 ;
}

#endif
