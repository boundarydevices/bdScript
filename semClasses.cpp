/*
 * Module semClasses.cpp
 *
 * This module defines the methods of the semaphore
 * classes as declared in semClasses.h
 *
 *
 * Change History : 
 *
 * $Log: semClasses.cpp,v $
 * Revision 1.1  2002-12-02 14:59:09  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "semClasses.h"

static pthread_mutex_t globalMutex_ = PTHREAD_MUTEX_INITIALIZER ;

mutex_t :: mutex_t( char const *name ) 
   : handle_(),
     owner_( -1UL ),
     lockCount_( 0 ),
     name_( name )
{ 
   pthread_mutex_t h2 = PTHREAD_MUTEX_INITIALIZER ; 
   handle_ = h2 ; 
}

mutex_t :: ~mutex_t( void )
{ 
   pthread_mutex_destroy( &handle_ ); 
}

mutexLock_t :: mutexLock_t( mutex_t &m )
   : mutex_( m ), 
     result_( -1 )
{
   int err = pthread_mutex_lock( &globalMutex_ );
   if( 0 == err )
   {
      if( ( 0 < m.lockCount_ ) && ( m.owner_ == pthread_self() ) )
      {
         ++m.lockCount_ ;
         result_ = 0 ;
         pthread_mutex_unlock( &globalMutex_ );
      } // nested lock
      else
      {
         pthread_mutex_unlock( &globalMutex_ );

         result_ = pthread_mutex_lock( &m.handle_ );
         err = pthread_mutex_lock( &globalMutex_ );
         if( 0 == err )
         {
            m.lockCount_ = 1 ;
            m.owner_ = pthread_self();

            pthread_mutex_unlock( &globalMutex_ );
         }
         else if( 0 == result_ )
         {
            pthread_mutex_unlock( &m.handle_ );
            result_ = -2 ;
         } // error locking global mutex??
      } // not already locked
   }
   else
      result_ = err ;
}

mutexLock_t :: ~mutexLock_t( void )
{
   if( 0 == result_ )
   {
      int err = pthread_mutex_lock( &globalMutex_ );
      if( 0 == err )
      {
         if( pthread_self() == mutex_.owner_ )
         {
            if( 0 == --mutex_.lockCount_ )
            {
               mutex_.owner_ = -1UL ;
               pthread_mutex_unlock( &mutex_.handle_ );
            } // no more refs
         }
         else
         {
            result_ = -1 ; // how?
         }
         pthread_mutex_unlock( &globalMutex_ );
      } // locked global
      else
      {
         result_ = -1 ; // how?
      }
   } // have a lock
}

#ifdef __STANDALONE__

static MUTEX( mutex_, "testMutex" );

unsigned long globalIterations = 0 ;

void nest( void )
{
   mutexLock_t lock( mutex_ );
}

void *thread1( void *param )
{
   unsigned long iterations = 0 ;
   while( 1000000 > iterations )
   {
      mutexLock_t lock( mutex_ );
      if( 3 == ( iterations & 3 ) )
      {
         nest();
      }
      iterations++ ;
      globalIterations++ ;
   }
   printf( "thread1 : %lu iterations\n", iterations );
}

void *thread2( void *param )
{
   unsigned long iterations = 0 ;
   while( 1000000 > iterations )
   {
      mutexLock_t lock( mutex_ );
      iterations++ ;
      globalIterations++ ;
      if( 3 == ( iterations & 3 ) )
      {
         nest();
      }
   }
   printf( "thread2 : %lu iterations\n", iterations );
}

int main( void )
{
   pthread_t first ;
   int create = pthread_create( &first, 0, thread1, 0 );
   if( 0 == create )
   {
      pthread_t second ;
      create = pthread_create( &second, 0, thread2, 0 );
      if( 0 == create )
      {
         void *exitStat ;
         int done = pthread_join( first, &exitStat );
         if( 0 == done )
         {
            done = pthread_join( second, &exitStat );
            if( 0 == done )
            {
               printf( "both threads completed, iterations %lu\n", globalIterations );
            }
         }
      }
   }

   return 0 ;
}

#endif
