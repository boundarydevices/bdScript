#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__ "$Id: semClasses.h,v 1.4 2002-11-30 00:30:30 ericn Exp $"

/*
 * semaphore.h
 *
 * This header file declares a couple of semaphore
 * wrappers for the pthread library.
 *
 *
 * Change History : 
 *
 * $Log: semClasses.h,v $
 * Revision 1.4  2002-11-30 00:30:30  ericn
 * -added semaphore_t class
 *
 * Revision 1.3  2002/11/17 16:08:48  ericn
 * -fixed timed-out semaphore handling
 *
 * Revision 1.2  2002/10/31 02:04:42  ericn
 * -modified to compile on ARM
 *
 * Revision 1.1  2002/10/27 17:42:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <stdio.h>

class mutex_t {
public:
   mutex_t( void ) : handle_(){ pthread_mutex_t h2 = PTHREAD_MUTEX_INITIALIZER ; handle_ = h2 ; }
   ~mutex_t( void ){ pthread_mutex_destroy( &handle_ ); }

   pthread_mutex_t handle_ ;

private:
   mutex_t( mutex_t & ); // no copies
};

class mutexLock_t {
public:
   mutexLock_t( mutex_t &m ) : handle_( m.handle_ ), result_( pthread_mutex_lock( &handle_ ) ){}
   mutexLock_t( pthread_mutex_t &m ) : handle_( m ), result_( pthread_mutex_lock( &handle_ ) ){}
   inline ~mutexLock_t( void );

   bool worked( void ) const { return 0 == result_ ; }

   pthread_mutex_t &handle_ ;
   int              result_ ;

private:
   mutexLock_t( mutexLock_t & ); // no copies
};


class condition_t {
public:
   //
   // should we include a mutex here to avoid it in wait calls 
   // and prevent multiple-mutex bugs?
   //
   condition_t( void ){ pthread_cond_init( &handle_, 0 ); }
   ~condition_t( void ){ pthread_cond_destroy( &handle_ ); }

   //
   // wait-side routines. 
   //
   // Note that these expect a locked mutex on input, 
   // and have the mutex locked on return. (unlocked between)
   //
   inline bool wait( mutexLock_t &mutex );
   inline bool wait( mutexLock_t &mutex,
                     unsigned long milliseconds );

   //
   // set-side routines
   //
   bool signal( void ){ return 0 == pthread_cond_signal( &handle_ ); }
   bool broadcast( void ){ return 0 == pthread_cond_broadcast( &handle_ ); }

   pthread_cond_t handle_ ;

private:
   condition_t( condition_t & ); // no copies
};


class semaphore_t {
public:
   semaphore_t( void ){ sem_init( &sem_, 0, 0 ); }
   ~semaphore_t( void ){ sem_destroy( &sem_ ); }

   inline bool wait( void ){ return 0 == sem_wait( &sem_ ); }
   inline bool signal( void ){ return 0 == sem_post( &sem_ ); }

   sem_t sem_ ;
};

mutexLock_t :: ~mutexLock_t( void )
{ 
   if( 0 == result_ ) 
   {
//      int rval = 
      pthread_mutex_unlock( &handle_ ); 
//      if( 0 != rval )
//         printf( "Error unlocking mutex!\n" );
   }
}

bool condition_t :: wait( mutexLock_t &lock )
{ 
   if( 0 == pthread_cond_wait( &handle_, &lock.handle_ ) )
   {
      return true ;
   }
   else
   {
      // note that pthread_cond_wait re-acquired lock before returning
      return false ;
   }
}

inline bool condition_t :: wait
   ( mutexLock_t  &lock,
     unsigned long milliseconds )
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
   
   if( 0 == pthread_cond_timedwait( &handle_, &lock.handle_, &then ) )
   {
      return true ;
   }
   else
   {
      // note that pthread_cond_timedwait re-acquired lock before returning
      return false ;
   }
}


#endif

