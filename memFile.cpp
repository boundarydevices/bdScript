/*
 * Module memFile.cpp
 *
 * This module defines the memFile_t class as declared
 * in memFile.h
 *
 *
 * Change History : 
 *
 * $Log: memFile.cpp,v $
 * Revision 1.4  2007-08-23 00:25:49  ericn
 * -add CLOEXEC for file handles
 *
 * Revision 1.3  2003/07/24 13:44:39  ericn
 * -set CLOEXEC
 *
 * Revision 1.2  2002/10/09 01:09:11  ericn
 * -added copy constructor
 *
 * Revision 1.1  2002/10/07 04:38:20  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "memFile.h"
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>

memFile_t :: memFile_t( char const *path )
   : fd_( open( path, O_RDONLY ) ),
     data_( 0 ),
     length_( 0 ),
     errno_( 0 )
{
   if( 0 <= fd_ )
   {
      fcntl( fd_, F_SETFD, FD_CLOEXEC );
      off_t const fileSize = lseek( fd_, 0, SEEK_END );
      if( 0 <= fileSize )
      {
         lseek( fd_, 0, SEEK_SET );
         void *mem = mmap( 0, fileSize, PROT_READ, MAP_PRIVATE, fd_, 0 );
         if( MAP_FAILED != mem )
         {
            data_   = mem ;
            length_ = fileSize ;
            return ;
         }
         else
            errno_ = errno ;
      }
      else
         errno_ = errno ;

      close( fd_ );
      fd_ = -1 ;
   }
   else
      errno_ = errno ;
}

memFile_t :: memFile_t( memFile_t const &rhs )
   : fd_( dup( rhs.fd_ ) ),
     data_( ( 0 <= fd_ ) ? mmap( 0, rhs.length_, PROT_READ, MAP_PRIVATE, fd_, 0 ) : 0 ),
     length_( rhs.length_ ),
     errno_( errno )
{
   if( 0 <= fd_ ){
      fcntl( fd_, F_SETFD, FD_CLOEXEC );
   }

   if( ( 0 == data_ ) && ( 0 <= fd_ ) )
   {
      close( fd_ );
      fd_ = -1 ;
   } // mmap failed
}

memFile_t :: ~memFile_t( void )
{
   if( 0 != data_ )
   {
      munmap( (void *)data_, length_ );
   }
   if( 0 <= fd_ )
   {
      close( fd_ );
   }
}
   
char const *memFile_t :: getError( void ) const 
{
   return strerror( errno_ );
}

