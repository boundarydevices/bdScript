/*
 * Module pollTimer.cpp
 *
 * This module defines the methods of the pollTimer_t
 * class, as well as a timer-based pollHandler_t for
 * use in implementing it.
 *
 *
 * Change History : 
 *
 * $Log: pollTimer.cpp,v $
 * Revision 1.4  2006-12-01 18:44:22  tkisky
 * -compiler warnings
 *
 * Revision 1.3  2006/10/07 16:13:43  ericn
 * -added include
 *
 * Revision 1.2  2004/09/09 21:02:36  tkisky
 * -error msg /dev/timer not found
 *
 * Revision 1.1  2003/12/27 18:38:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "pollTimer.h"
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <linux/gen-timer.h>
#include <sys/ioctl.h>
#include <poll.h>
#include "tickMs.h"

class timerPollHandler_t;
static timerPollHandler_t* timer_ = NULL ;

class timerPollHandler_t : public pollHandler_t {
public:
   timerPollHandler_t( int fd, pollHandlerSet_t &set )
      : pollHandler_t( fd, set )
      , head_( 0 )
   {
      setMask( POLLIN );
      set.add( *this );
   }

   ~timerPollHandler_t( void ){}

   virtual void onDataAvail( void );     // POLLIN

   //
   // adds a timers to the list of active (pending) timers
   //
   void add( pollTimer_t &, unsigned long msFromNow );
   void remove( pollTimer_t & );

private:
   void updateExpiration( void );

   pollTimer_t *head_ ;

};

void timerPollHandler_t :: updateExpiration( void )
{
   if( head_ )
   {
      ioctl( getFd(), SET_TSTAMP, &head_->expiration_ );
   }
   else
      ioctl( getFd(), CLEAR_TSTAMP, 0 );
}

void timerPollHandler_t :: onDataAvail( void )
{
   bool fired = false ;
   long long const now = tickMs();
   while( ( 0 != head_ )
          &&
          ( 0 <= (now-head_->expiration_) ) )
   {
      pollTimer_t *temp = head_ ;
      temp->clear();
      temp->fire();
      fired = true ;
   }
   if( !fired )
      printf( "idle onDataAvail()\n" );
   updateExpiration();
}

void timerPollHandler_t :: add( pollTimer_t &pt, unsigned long msFromNow )
{
   assert( false == pt.isActive_ );
   pt.expiration_ = tickMs() + msFromNow ;
   pt.isActive_ = true ;

   pollTimer_t *front = head_ ;
   pollTimer_t *rear  = 0 ;
   while( front )
   {
      if( 0 > ( front->expiration_ - pt.expiration_ ) )
      {
         rear  = front ;
         front = rear->next_ ;
      }
      else
         break;
   }

   pt.next_ = front ;
   if( 0 == rear )
   {
      head_ = &pt ;
   }
   else
      rear->next_ = &pt ;

   updateExpiration();
}

void timerPollHandler_t :: remove( pollTimer_t &pt )
{
   pollTimer_t *front = head_ ;
   pollTimer_t *rear  = 0 ;
   while( front )
   {
      if( &pt != front )
      {
         rear  = front ;
         front = rear->next_ ;
      }
      else
         break;
   }

   assert( 0 != front );

   if( 0 == rear )
   {
      head_ = front->next_ ;
   }
   else
      rear->next_ = front->next_ ;

   updateExpiration();
}

pollHandler_t &getTimerPoll( pollHandlerSet_t &set )
{
   if( 0 == timer_ ) {
      int fd = open( "/dev/timer", O_RDONLY|O_NONBLOCK );
      if (fd >= 0) timer_ = new timerPollHandler_t( fd,set );
      else printf("!!!!!device /dev/timer not found\n");
   }
   return *timer_ ;
}

pollTimer_t :: pollTimer_t( void )
   : expiration_( 0 )
   , isActive_( false )
   , next_( 0 )
{
   assert( 0 != timer_ );
}

pollTimer_t :: ~pollTimer_t( void )
{
   clear();
}

void pollTimer_t :: set( unsigned long msFromNow )
{
   clear();
   timer_->add( *this, msFromNow );
}

void pollTimer_t :: clear( void )
{
   if( isActive_ )
   {
      timer_->remove( *this );
      isActive_ = false ;
   }
}

void pollTimer_t :: fire( void )
{
   printf( "pollTimer_t::fire()\n" );
}

#ifdef STANDALONE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int main( void )
{
   pollHandlerSet_t handlers ;
   (void)getTimerPoll( handlers );

   pollTimer_t timer1 ;
   printf( "should not fire here\n" );
   timer1.set( 1000 );
   timer1.clear();

   printf( "should fire in 2 seconds\n" );
   timer1.set( 2000 );
   if( handlers.poll( -1 ) )
      printf( "at least one fired\n" );
   else
      printf( "nothing fired\n" );
   printf( "done waiting\n" );

   pollTimer_t timer2 ;
   timer2.set( 1000 );
   timer1.set( 1000 );

   printf( "two should fire here\n" );
   if( handlers.poll( -1 ) )
      printf( "at least one fired\n" );
   else
      printf( "nothing fired\n" );

   printf( "nothing should fire here\n" );
   if( handlers.poll( 3000 ) )
      printf( "???? Huh ???? - something fired\n" );
   else
      printf( "nothing fired\n" );

   printf( "nothing should fire here\n" );
   if( handlers.poll( 3000 ) )
      printf( "???? Huh ???? - something fired\n" );
   else
      printf( "nothing fired\n" );

   return 0 ;
}

#endif
