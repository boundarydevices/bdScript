#ifndef __MTQUEUE_H__
#define __MTQUEUE_H__ "$Id: mtQueue.h,v 1.7 2002-12-02 15:06:59 ericn Exp $"

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
 * Revision 1.7  2002-12-02 15:06:59  ericn
 * -removed debug stuff
 *
 * Revision 1.6  2002/12/02 15:05:54  ericn
 * -removed use of mutex_t (because of nesting)
 *
 * Revision 1.5  2002/11/30 05:28:22  ericn
 * -fixed timed wait, added isEmpty()
 *
 * Revision 1.4  2002/11/30 00:52:59  ericn
 * -changed name to semClasses.h
 *
 * Revision 1.3  2002/11/29 16:42:21  ericn
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

#include "semClasses.h"
#include <list>

template <class T>
class mtQueue_t {
public:
   mtQueue_t( void ) : mutex_(), cond_(), list_(), abort_( false )
   {
      pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER ; 
      mutex_ = m2 ; 
      pthread_cond_init( &cond_, 0 );
   }
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

   inline bool isEmpty( void );

private:
   mtQueue_t( mtQueue_t const & ); // no copies
   pthread_mutex_t   mutex_ ;
   pthread_cond_t    cond_ ;
   std::list<T>      list_ ;
   bool volatile     abort_ ;
};

//
// reader-side 
//
template <class T>
bool mtQueue_t<T>::pull( T &item )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      if( !abort_ && list_.empty() )
      {
         while( 0 == pthread_cond_wait( &cond_, &mutex_ ) )
         {
            if( !list_.empty() )
            {
               item = list_.front();
               list_.pop_front();
               pthread_mutex_unlock( &mutex_ );
               return true ;
            }
            else if( abort_ )
            {
               pthread_mutex_unlock( &mutex_ );
               return false ;
            }
         }
         
         pthread_mutex_unlock( &mutex_ );
         return false ;
      }
      else if( !abort_ )
      {
         item = list_.front();
         list_.pop_front();
         pthread_mutex_unlock( &mutex_ );
         return true ;
      } // no need to wait
      else
      {
         pthread_mutex_unlock( &mutex_ );
         return false ;
      }
   }
   else
   {
      fprintf( stderr, "mtQueue aborting(lock)\n" );
      return false ;
   }
}
   
template <class T>
bool mtQueue_t<T>::pull( T &item, unsigned long milliseconds )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      if( !abort_ && list_.empty() )
      {
         struct timeval now ; gettimeofday( &now, 0 );
         now.tv_usec += milliseconds*1000 ;
         if( 1000000 < now.tv_usec )
         {
            now.tv_sec  += now.tv_usec / 1000000 ;
            now.tv_usec  = now.tv_usec % 1000000 ;
         }
         timespec then ; 
         
         then.tv_sec  = now.tv_sec ;
         then.tv_nsec = now.tv_usec*1000 ;
         
         while( 0 == pthread_cond_timedwait( &cond_, &mutex_, &then ) )
         {
            if( !abort_ && !list_.empty() )
            {
               item = list_.front();
               list_.pop_front();
               pthread_mutex_unlock( &mutex_ );
               return true ;
            }
            else if( abort_ )
            {
               pthread_mutex_unlock( &mutex_ );
               return false ;
            }
         }

         pthread_mutex_unlock( &mutex_ );

         return false ;
      }
      else
      {
         item = list_.front();
         list_.pop_front();
         pthread_mutex_unlock( &mutex_ );
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
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      list_.push_back( item );
      pthread_cond_signal( &cond_ );
      pthread_mutex_unlock( &mutex_ );
      return true ;
   }
   else
   {
      return false ;
   }
}
   
template <class T>
bool mtQueue_t<T>::abort( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   if( 0 == err )
   {
      abort_ = true ;
      pthread_cond_broadcast( &cond_ );
      pthread_mutex_unlock( &mutex_ );
      return true ;
   }
   else
      return false ;
}
template <class T>
bool mtQueue_t<T>::isEmpty( void )
{
   int err = pthread_mutex_lock( &mutex_ );
   
   bool result = list_.empty();
   
   if( 0 == err )
      pthread_mutex_unlock( &mutex_ );

   return result ;
}

#endif

