/*
 * Module videoQueue.cpp
 *
 * This module defines the methods of the videoQueue_t
 * structure as declared in videoQueue.h
 *
 *
 * Change History : 
 *
 * $Log: videoQueue.cpp,v $
 * Revision 1.1  2003-07-27 15:19:12  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "videoQueue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#define NUMENTRIES 10

#ifdef _WIN32
#define I64FMT "%I64u"
#else
#define I64FMT "%llu"
#endif

videoQueue_t :: videoQueue_t
   ( unsigned width,
     unsigned height )
   : width_( width ),
     height_( height ),
     rowStride_( width*sizeof(unsigned short) ),
     entrySize_( (offsetof(entry_t,data_)+(((rowStride_*height+3)/4)*4)) ),
     memSize_( NUMENTRIES*entrySize_ ),
     discards_( 0 ),
     mem_( malloc( memSize_ ) )
{
   full_.next_  = full_.prev_ = &full_ ;
   empty_.next_ = empty_.prev_ = &empty_ ;
   entry_t *nextEntry = (entry_t *)mem_ ;
   entry_t *end = (entry_t *)( (unsigned char *)mem_ + memSize_ );
   while( nextEntry < end )
   {
      memset( nextEntry, 0, entrySize_ );
      putEmpty( nextEntry );
      nextEntry = (entry_t *)( (unsigned char *)nextEntry + entrySize_ );
   }
}

videoQueue_t :: ~videoQueue_t( void )
{
    if( mem_ )
        free( mem_ );
}

#ifdef MODULETEST

int main( void )
{
   videoQueue_t queue( 320, 240 );
   printf( "entry size %u\n", queue.entrySize_ );
   printf( "row stride %u\n", queue.rowStride_ );

   unsigned idx = NUMENTRIES ;
   videoQueue_t::entry_t *entry ;

   while( 0 != ( entry = queue.getEmpty() ) )
   {
      printf( "empty %u, %p\n", idx, entry );
      entry->when_ms_ = idx-- ;
      queue.putFull( entry );
   }

   while( 0 != ( entry = queue.getFull() ) )
   {
      printf( "full " I64FMT ", %p\n", entry->when_ms_, entry );
      queue.putEmpty( entry );
   }
   
   return 0 ;
}
#endif

