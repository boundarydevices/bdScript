#ifndef __YUVQUEUE_H__
#define __YUVQUEUE_H__ "$Id: yuvQueue.h,v 1.1 2005-04-24 18:55:03 ericn Exp $"

/*
 * yuvQueue.h
 *
 * This header file declares the yuvQueue_t data structure, 
 * to keep track of yuv frames for output.
 * 
 * It consists of a carved-up memory buffer for
 * appropriately sized frames.
 *
 * It keeps track of two sets of frames, full and empty.
 *
 * The full list is kept doubly-linked because items
 * are added to the tail, and may need placement which
 * is not FIFO.
 *
 * The empty list is kept as a singly-linked list and 
 * items are both added and removed from the 'head' in
 * LIFO order to increase cache efficiency.
 *
 * None of this is threadsafe.
 *
 * Change History : 
 *
 * $Log: yuvQueue.h,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 * Revision 1.1  2003/07/27 15:19:12  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include <stddef.h>

#if 0
struct yuvQueue_t {
   yuvQueue_t( unsigned width,
               unsigned height,
               unsigned framesPerAlloc );
   ~yuvQueue_t( void );

   struct entry_t {
      entry_t      *next_ ;
      entry_t      *prev_ ;
      long long     when_ms_ ;
      unsigned      type_ ; // mpegDecoder_t::type_e
      unsigned char data_[1];
   };

   struct page_t {
      page_t        *next_ ;
      entry_t        entries_[1];
   };

   //
   // filler interface
   //
   //
   inline bool isFull( void ) const { return empty_.next_ == &empty_ ; }
   inline entry_t *getEmpty( void );
   inline void putFull( entry_t *e );
   inline static entry_t *entryFromData( void *data );

   //
   // player interface
   //
   inline bool      isEmpty( void ) const { return full_.next_ == &full_ ; }
   inline entry_t  *getFull( void );
   inline void      putEmpty( entry_t *e );
   inline long long frontPTS( void );

private:
   // 
   // internals (don't look)
   //

   void allocPage( void );

   unsigned const width_ ;
   unsigned const height_ ;
   unsigned const framesPerAlloc_ ;
   unsigned       rowStride_ ;
   unsigned const entrySize_ ;
   unsigned const memSize_ ;
   unsigned       discards_ ;
   page_t        *pages_ ;
   entry_t        full_ ;
   entry_t        empty_ ;
};


yuvQueue_t :: entry_t *yuvQueue_t :: getEmpty( void )    // returns zero if none are present
{
   if( isFull() )
      allocPage();
   
   entry_t *rval = empty_.next_ ;
   empty_.next_ = rval->next_ ;

   return rval ;
}

void yuvQueue_t :: putFull( entry_t *e )
{
   //
   // walk until e->when_ms_ is greater than prev
   //
   entry_t *tail = full_.prev_ ;
   while( tail != &full_ )
   {
      long diff = ( tail->when_ms_ - e->when_ms_ );
//      if( 0 < diff )
      if( 0 <= diff )
      {
         break;
      } // add this entry after tail
/*
      else if( 0 == diff )
      {
         putEmpty( e );
         return ;
      } // same time as tail
*/
      else
      {
         tail = tail->prev_ ;
      }
   }

   e->next_ = tail->next_ ;
   tail->next_ = e ;
   e->next_->prev_ = e ;
   e->prev_ = tail ;
}

   //
   // player interface
   //
yuvQueue_t :: entry_t *yuvQueue_t :: getFull( void )
{
   entry_t *rval ;
   if( !isEmpty() )
   {
      rval = full_.next_ ;
      full_.next_       = rval->next_ ;
      rval->next_->prev_ = &full_ ;
   }
   else
      rval = 0 ;

   return rval ;
}

void yuvQueue_t :: putEmpty( entry_t *e )
{
   e->next_ = empty_.next_ ;
   empty_.next_ = e ;
}

long long yuvQueue_t :: frontPTS( void )
{
   if( !isEmpty() )
   {
      return full_.next_->when_ms_ ;
   }
   else
      return 0x7fffffff ;
}

inline yuvQueue_t :: entry_t *yuvQueue_t :: entryFromData( void *data )
{
   char *dataBytes = (char *)data ;
//   return (entry_t *)( dataBytes - (unsigned)(((entry_t*)0)->data_ ));
   return (entry_t *)( dataBytes - offsetof(entry_t,data_) );
}
#else

#include "mpegDecode.h"
#include <sys/time.h>

struct yuvQueue_t {
   yuvQueue_t( int      fd,
               unsigned inWidth,
               unsigned inHeight,
               unsigned xLeft,
               unsigned yTop,
               unsigned outWidth,
               unsigned outHeight );
   ~yuvQueue_t( void );

   struct entry_t {
      unsigned       idx_ ;
      unsigned long  when_ ;
      unsigned char *data_ ;
   };

   //
   // filler interface
   //
   //
   inline bool isFull( void );
   inline entry_t *getEmpty( void );
   inline void putFull( entry_t  *e, 
                        long long whenMs,
                        mpegDecoder_t::picType_e type );
   inline void setStartMs( unsigned long when ){ startMs_ = when ; }

private:
   // 
   // internals (don't look)
   //
   int            fd_ ;
   unsigned const width_ ;
   unsigned const height_ ;
   unsigned       rowStride_ ;
   unsigned       entrySize_ ;
   unsigned       memSize_ ;
   void          *mem_ ;
   void         **frames_ ;
   entry_t        entry_ ;
   void          *frameData_ ;
   entry_t        entries_[32];
   unsigned       nextEntry_ ;
   unsigned long  minMs_ ;
   unsigned long  maxMs_ ;
   unsigned long  startMs_ ;
};

#endif 

#endif

