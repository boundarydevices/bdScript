#ifndef __MTQUEUE_H__
#define __MTQUEUE_H__ "$Id: mtQueue.h,v 1.1 2002-10-27 17:42:08 ericn Exp $"

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
 * Revision 1.1  2002-10-27 17:42:08  ericn
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
   mtQueue_t( void ) : mutex_(), cond_(), list_(){}
   ~mtQueue_t( void ){}

   //
   // reader-side 
   //
   inline bool pull( T &item );
   inline bool pull( T &item, unsigned long milliseconds );

   //
   // writer side
   //
   inline bool push( T const &item );
   mutex_t        mutex_ ;
   condition_t    cond_ ;
   std::list<T>   list_ ;
};

//
// reader-side 
//
template <class T>
bool mtQueue_t<T>::pull( T &item )
{
   mutexLock_t lock( mutex_ );
   if( lock.worked() )
   {
      if( list_.empty() )
      {
         if( cond_.wait( lock ) )
         {
            item = list_.front();
            list_.pop_front();
            return true ;
         }
         else
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
   
template <class T>
bool mtQueue_t<T>::pull( T &item, unsigned long milliseconds )
{
   mutexLock_t lock( mutex_ );
   if( lock.worked() )
   {
      if( list_.empty() )
      {
         if( cond_.wait( lock, milliseconds ) )
         {
            item = list_.front();
            list_.pop_front();
            return true ;
         }
         else
         {
            return false ;
         }
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

#endif

