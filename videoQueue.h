#ifndef __VIDEOQUEUE_H__
#define __VIDEOQUEUE_H__ "$Id: videoQueue.h,v 1.2 2006-08-31 15:49:13 ericn Exp $"

/*
 * videoQueue.h
 *
 * This header file declares the videoQueue_t 
 * data structure, which is used mostly for 
 * convenience and testing.
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
 * $Log: videoQueue.h,v $
 * Revision 1.2  2006-08-31 15:49:13  ericn
 * -align for DMA
 *
 * Revision 1.1  2003/07/27 15:19:12  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


struct videoQueue_t {
   videoQueue_t( unsigned width,
                 unsigned height );
   ~videoQueue_t( void );

   struct entry_t {
      entry_t      *next_ ;
      entry_t      *prev_ ;
      long long     when_ms_ ;
      unsigned      type_ ;  // mpegDecoder_t::type_e
      unsigned      pad_[1]; // align on 8-byte boundary
      unsigned char data_[1];
   };

   //
   // filler interface
   //
   //
   inline bool isFull( void ) const { return empty_.next_ == &empty_ ; }
   inline entry_t *getEmpty( void );   // returns zero if none are present
   inline void putFull( entry_t *e );

   //
   // player interface
   //
   inline bool     isEmpty( void ) const { return full_.next_ == &full_ ; }
   inline entry_t *getFull( void );
   inline void putEmpty( entry_t *e );
   inline long long frontPTS( void );

   unsigned const width_ ;
   unsigned const height_ ;
   unsigned       rowStride_ ;
   unsigned const entrySize_ ;
   unsigned const memSize_ ;
   unsigned       discards_ ;
   void *const    mem_ ;
   entry_t        full_ ;
   entry_t        empty_ ;
};


videoQueue_t :: entry_t *videoQueue_t :: getEmpty( void )    // returns zero if none are present
{
   entry_t *rval ;
   if( !isFull() )
   {
      rval = empty_.next_ ;
      empty_.next_ = rval->next_ ;
   }
   else
      rval = 0 ;

   return rval ;
}

void videoQueue_t :: putFull( entry_t *e )
{
   //
   // walk until e->when_ms_ is greater than prev
   //
   entry_t *tail = full_.prev_ ;
/*
   while( tail != &full_ )
   {
      if( tail->when_ms_ <= e->when_ms_ )
      {
         break;
      }
      else
      {
         tail = tail->prev_ ;
      }
   }

*/

// throw out any late frames
/*
   while( tail != &full_ )
   {
      if( tail->when_ms_ <= e->when_ms_ )
      {
         break;
      } // found our spot
      else
      {
         entry_t *discard = tail ;
         tail = discard->prev_ ;
         discard->prev_->next_ = discard->next_ ;
         discard->next_->prev_ = discard->prev_ ;
         putEmpty( discard );
         ++discards_ ;
      }
   }
*/

   e->next_ = tail->next_ ;
   tail->next_ = e ;
   e->next_->prev_ = e ;
   e->prev_ = tail ;
}

   //
   // player interface
   //
videoQueue_t :: entry_t *videoQueue_t :: getFull( void )
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

void videoQueue_t :: putEmpty( entry_t *e )
{
   e->next_ = empty_.next_ ;
   empty_.next_ = e ;
}

long long videoQueue_t :: frontPTS( void )
{
   if( !isEmpty() )
   {
      return full_.next_->when_ms_ ;
   }
   else
      return 0x7fffffff ;
}

#endif

