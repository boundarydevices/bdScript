#ifndef __MTQUEUE_H__
#define __MTQUEUE_H__ "$Id: mtQueue.h,v 1.3 2002-11-29 16:42:21 ericn Exp $"

/*
 * mtQueue.h
 *
 * This header file declares the mtQueue_t template class, 
 * which combines an STL list with mutex and event semaphores
 * to allow multiple readers and writers.
 *
 * Note that the items placed in the queue must have copy 
 * semantics.
 *
 * Change History : 
 *
 * $Log: mtQueue.h,v $
 * Revision 1.3  2002-11-29 16:42:21  ericn
 * -modified to allow for unproductive wakes
 *
 * Revision 1.2  2002/10/31 02:05:54  ericn
 * -added abort method to wake up threads (pthread_cancel doesn't wake 'em)
 *
 * Revision 1.1  2002/10/27 17:42:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "semaphore.h"
#include <list>

template <class T>
class mtQueue_t {
public:
   mtQueue_t( void ) : mutex_(), cond_(), list_(), abort_( false ){}
   ~mtQueue_t( void ){}

   //
   // reader-side. returns false on abort
   //
   inline bool pull( T &item );
   inline bool pull( T &item, unsigned long milliseconds );

   //
   // writer side
   //
   inline bool push( T const &item );
   
   //
   // abort (meaningful to threads)
   //
   inline bool abort( void );

   mutex_t        mutex_ ;
   condition_t    cond_ ;
   std::list<T>   list_ ;
   bool volatile  abort_ ;
};

//
// reader-side 
//
template <class T>
bool mtQueue_t<T>::pull( T &item )
{
//   printf( "locking\n" );
   mutexLock_t lock( mutex_ );
   if( lock.worked() )
   {
      if( !abort_ && list_.empty() )
      {
//   printf( "waiting\n" );
         while( cond_.wait( lock ) )
         {
            if( !list_.empty() )
            {
               item = list_.front();
               list_.pop_front();
//   printf( "returning\n" );
               return true ;
            }
            else if( abort_ )
               return false ;
         }
         
         printf( "aborting(wait)\n" );
         return false ;
      }
      else if( !abort_ )
      {
//   printf( "returning immediately\n" );
         item = list_.front();
         list_.pop_front();
         return true ;
      } // no need to wait
      else
         return false ;
   }
   else
   {
      printf( "aborting(lock)\n" );
      return false ;
   }
}
   
template <class T>
bool mtQueue_t<T>::pull( T &item, unsigned long milliseconds )
{
   mutexLock_t lock( mutex_ );
   if( lock.worked() )
   {
      if( !abort_ && list_.empty() )
      {
         while( cond_.wait( lock, milliseconds ) )
         {
            if( !abort_ && !list_.empty() )
            {
               item = list_.front();
               list_.pop_front();
            }
            else if( abort_ )
               return false ;
         }

         return false ;
      }
      else
      {
         item = list_.front();
         list_.pop_front();
         return true ;
      } // no need to wait
   }
   else
      return false ;
}

//
// writer side
//
template <class T>
bool mtQueue_t<T>::push( T const &item )
{
   mutexLock_t lock( mutex_ );
   if( lock.worked() )
   {
      list_.push_back( item );
      cond_.signal();
      return true ;
   }
   else
      return false ;
}
   
template <class T>
bool mtQueue_t<T>::abort( void )
{
   mutexLock_t lock( mutex_ );
   if( lock.worked() )
   {
      abort_ = true ;
      cond_.broadcast();
      return true ;
   }
   else
      return false ;
}

#endif

