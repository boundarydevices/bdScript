/*
 * Module fbMem.cpp
 *
 * This module defines the methods of the fbMemory_t
 * class as declared in fbMemory.h
 *
 *
 * Change History : 
 *
 * $Log: fbMem.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbMem.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/sm501mem.h>
#include "fbDev.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "debugPrint.h"

static fbMemory_t *instance_ = 0 ;
static unsigned long numAlloc_ = 0 ;
static unsigned long numFree_ = 0 ;

fbMemory_t &fbMemory_t::get( void )
{
   if( 0 == instance_ )
      instance_ = new fbMemory_t ;
   return *instance_ ;
}
   
void fbMemory_t::destroy( void )
{
   if( instance_ ){
      fbMemory_t *tmp = instance_ ;
      instance_ = 0 ;
      delete tmp ;
   }
}


unsigned fbMemory_t::alloc( unsigned size )
{
   unsigned const inSize = size ;
   int rval = ioctl( fd_, SM501_ALLOC, &size );
   if( 0 == rval ){
      debugPrint( "allocate %u bytes: %x\n", inSize, size );
      ++numAlloc_ ;
      unsigned offs = size ;
      return offs ;
   }
   
   fprintf( stderr, "SM501_ALLOC error, %u bytes\n", size );
   exit(0);

   return 0;
}

void fbMemory_t::free( unsigned offs )
{
   debugPrint( "free %x\n", offs );
   int rval = ioctl( fd_, SM501_FREE, &offs );
   if( rval )
      perror( "SM501_FREE" );
   ++numFree_ ;
}

fbMemory_t::fbMemory_t( void )
   : fd_( open( "/dev/sm501mem", O_RDWR ) )
{
   debugPrint( "opened fd %d\n", fd_ );
}


fbMemory_t::~fbMemory_t( void )
{
   if( 0 <= fd_ ){
      debugPrint( "closing fd %d\n", fd_ );
      close( fd_ );
      fd_ = -1 ;
   }
}


fbPtrImpl_t :: ~fbPtrImpl_t( void )
{
   assert( 0 == count_ );
   assert( 0 == offs_ );
   assert( 0 == ptr_ );
}


void fbPtrImpl_t :: releaseRef( void )
{
   debugPrint( "release instance %p\n", this );

   assert( 0 < count_ );
   --count_ ;
   if( 0 == count_ )
   {
      if( ptr_ && offs_ ){
         fbMemory_t::get().free(offs_);
         offs_ = 0 ;
         ptr_  = 0 ;
         size_ = 0 ;
      }
debugPrint( "last instance %p\n", this );
      delete this ;
   }
}

fbPtr_t :: fbPtr_t( void )
   : inst_( 0 )
{
}

fbPtr_t :: fbPtr_t( fbPtr_t const &r )
   : inst_( r.inst_ )
{
   debugPrint( "share instance %p\n", r.inst_ );

   if( inst_ )
      inst_->addRef();
}

fbPtr_t :: fbPtr_t( unsigned size )
{
   unsigned offs = fbMemory_t::get().alloc(size);
   if( offs ){
      void *ptr = offs ? (char *)getFB().getMem() + offs
                       : 0 ;
      inst_ = new fbPtrImpl_t( offs, ptr, size );
debugPrint( "new instance %p\n", inst_ );
   }
   else
      inst_ = 0 ;
}

fbPtr_t :: ~fbPtr_t( void )
{
   if( inst_ )
      inst_->releaseRef();
}

fbPtr_t &fbPtr_t::operator=( fbPtr_t const &rhs )
{
   if( inst_ )
      inst_->releaseRef();

   inst_ = rhs.inst_ ;
   if( inst_ )
      inst_->addRef();

   return *this ;
}

unsigned fbPtr_t :: size( void ) const 
{
   return inst_ ? inst_->size_ : 0 ;
}

#ifdef MODULETEST
#include <vector>
#include <stdio.h>
#include "tickMs.h"

int main( void )
{
   printf( "Hello, fbMem\n" );
   long long start, end ;

   {
      std::vector<fbPtr_t> copied ;

      {
         std::vector<fbPtr_t> allocations ;
      
         unsigned iteration = 0 ;
         unsigned alloc = 0 ;
         start = tickMs();
         while(1){
            unsigned const allocSize = ( iteration & 7 ) + 1 ;
            fbPtr_t ptr( allocSize );
            if( 0 == ptr.getPtr() )
               break ;
      
      //      printf( "Alloc: %x/%p\n", ptr.getOffs(), ptr.getPtr() );
            allocations.push_back(ptr);
            alloc += allocSize ;
            iteration++ ;
         }
         end = tickMs();
      
         printf( "allocated %u bytes in %u iterations, %lu ms\n", alloc, iteration, (unsigned long)(end-start) );
      
#if 0
         for( unsigned i = 0 ; i < allocations.size(); i++ )
         {
            fbPtr_t &ptr = allocations[i];
            printf( "[%5u] - %p/%x/%p/%u\n", 
                    i, ptr.inst_, 
                    ptr.inst_ ? ptr.inst_->offs_ : 0,
                    ptr.inst_ ? ptr.inst_->ptr_ : (void *)0,
                    ptr.inst_ ? ptr.inst_->count_ : 0 );
         }
#endif
         copied = allocations ;
      }

      start = tickMs();
   
   } // limit scope of vector

   end = tickMs();
   printf( "freed in %lu ms\n", (unsigned long)(end-start) );

   printf( "%lu allocs/ %lu frees\n", numAlloc_, numFree_ );

   return 0 ;
}

#endif
