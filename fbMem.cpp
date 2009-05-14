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
 * Revision 1.6  2009-05-14 16:26:18  ericn
 * [multi-display SM-501] Add frame-buffer offsets
 *
 * Revision 1.5  2007-08-23 00:28:19  ericn
 * -map memory separately from fb0
 *
 * Revision 1.4  2007/08/08 17:08:19  ericn
 * -[sm501] Use class names from sm501-int.h
 *
 * Revision 1.3  2006/10/19 00:32:57  ericn
 * -debugMsg(), not printf()
 *
 * Revision 1.2  2006/10/16 22:34:48  ericn
 * -added validate() method
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbMem.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/sm501-int.h>
#include <linux/sm501mem.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "dlList.h"
#include <sys/mman.h>

// #define DEBUGPRINT
#include "debugPrint.h"

static fbMemory_t *instance_ = 0 ;
static unsigned long numAlloc_ = 0 ;
static unsigned long numFree_ = 0 ;
static unsigned long fb0_offset ;

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
      debugPrint( "alloc fbRAM: %u bytes: 0x%x\n", inSize, size );
      ++numAlloc_ ;
      unsigned offs = size ;
      return offs ;
   }
   
   fprintf( stderr, "SM501_ALLOC error, %u bytes\n", size );

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
   : fd_( open( "/dev/" SM501MEM_CLASS, O_RDWR ) )
   , memBase_( 0 )
{
   debugPrint( "opened fd %d\n", fd_ );
   if( 0 <= fd_ ){
      fcntl(fd_, F_SETFD, FD_CLOEXEC);
      memBase_ = mmap( 0, SM501_FBMAX, PROT_WRITE|PROT_WRITE, MAP_SHARED, fd_, 0 );
      if( MAP_FAILED == memBase_ ){
         perror( "mmap sm501mem" );
         memBase_ = 0 ;
      }
      int rval = ioctl(fd_,SM501_BASEADDR,&fb0_offset);
      if( 0 != rval ){
         perror( "SM501_BASEADDR" );
         munmap(memBase_,SM501_FBMAX);
         memBase_ = 0 ;
      }
      debugPrint( "fbMemory_t::memBase_ == %p, fb0_offs %lu\n", memBase_, fb0_offset );
   }
   else
      perror( "/dev/" SM501MEM_CLASS );
}


fbMemory_t::~fbMemory_t( void )
{
   if( memBase_ ){
         munmap(memBase_,SM501_FBMAX);
         memBase_ = 0 ;
   }
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
debugPrint( "free RAM 0x%x (%u)\n", offs_, size_ );
         fbMemory_t::get().free(offs_);
         offs_ = 0 ;
         ptr_  = 0 ;
         size_ = 0 ;
      }
debugPrint( "last instance %p\n", this );
      delete this ;
   }
}


typedef struct {
   struct list_head node_ ;
   unsigned long    size_ ;
   unsigned long    pad_ ; // to 16-bytes
} allocHeader_t ;


bool fbPtrImpl_t :: validate( void ) const 
{
   if( 0 < count_ ){
      allocHeader_t *alloc = ((allocHeader_t *)ptr_)-1 ;
      if( alloc->size_ >= size_ ){
         if( alloc->node_.next != alloc->node_.prev ){
            if( ( 0 != alloc->node_.next )
                &&
                ( 0 != alloc->node_.prev ) ){
               return true ;
            }
            else
               printf( "null pointer in node %p/%p/%p\n", alloc, alloc->node_.next, alloc->node_.prev );
         }
         else {
            printf( "pointer linked to itself: %p/%p/%p\n", alloc, alloc->node_.next, alloc->node_.prev );
         }
      }
      else {
         printf( "Invalid sizes: %lu/%u\n", alloc->size_, size_ );
      }
   }
   else {
      printf( "validate on unreferenced ptr\n" );
   }

   return false ;
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
      void *ptr = offs ? (char *)fbMemory_t::get().memBase_ + offs + fb0_offset
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

bool fbPtr_t :: validate( void ) const {
   if( inst_ )
      return inst_->validate();
   else {
      debugPrint( "Valid, but NULL pointer\n" );
      return true ;
   }
}


#ifdef MODULETEST
#include <vector>
#include <stdio.h>
#include "tickMs.h"

int main( void )
{
   fbPtr_t myPtr(0x1000);
   if( myPtr.getPtr() ){
      printf( "allocated a page: %p\n", myPtr.getPtr() );
   } else {
      fprintf( stderr, "error allocating a page\n");
      return -1 ;
   }
   long long start, end ;

   {
      std::vector<fbPtr_t> copied ;

      {
         std::vector<fbPtr_t> allocations ;
      
         unsigned iteration = 0 ;
         unsigned alloc = 0 ;
         start = tickMs();
         while(1){
            unsigned const allocSize = ( iteration & 7 ) + 4096 ;
            fbPtr_t ptr( allocSize );
            if( 0 == ptr.getPtr() ){
               fprintf( stderr, "Alloc error on iteration %u\n", iteration );
               break ;
            }
      
            printf( "Alloc: %x/%p\n", ptr.getOffs(), ptr.getPtr() );
            memset(ptr.getPtr(),iteration,allocSize);
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
