/*
 * Module touchSignal.cpp
 *
 * This module defines the methods of the touchSignal_t 
 * class as declared in touchSignal.h
 *
 *
 * Change History : 
 *
 * $Log: touchSignal.cpp,v $
 * Revision 1.2  2006-08-28 18:23:33  ericn
 * -read the touch data
 *
 * Revision 1.1  2006/08/28 15:45:31  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "touchSignal.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include "rtSignal.h"
#include <signal.h>

struct ts_event  {   /* Used in UCB1x00 style touchscreens (the default) */
	unsigned short pressure;
	unsigned short x;
	unsigned short y;
	unsigned short pad;
	struct timeval stamp;
};

static char const *getTouchDev(void)
{
   char const *devName = "/dev/misc/touchscreen/ucb1x00" ;
   char const *envDev = getenv( "TSDEV" );
   if( 0 != envDev )
      devName = envDev ;
   return devName ;
}

static touchSignal_t *inst_ = 0 ;
static touchHandler_t handler_ = 0 ;

touchSignal_t &touchSignal_t::get( touchHandler_t handler )
{
   if( 0 == inst_ )
      inst_ = new touchSignal_t ;

   if( handler )
      handler_ = handler ;

   return *inst_ ;
}

static void touchHandler( int signo, siginfo_t *info, void *context )
{
   if( inst_ ){
      ts_event nextEvent ;
      ts_event event ;
      unsigned count = 0 ;
      int numRead ;

      timeval firstTime ;
      
      while( sizeof( nextEvent ) == ( numRead = read( info->si_fd, &nextEvent, sizeof( nextEvent ) ) ) )
      {
         if( 0 == count )
            firstTime = nextEvent.stamp ;
         event = nextEvent ;
         count++ ;
      }

      if( 0 < count )
      {
         if( handler_ )
            handler_( event.x, event.y, event.pressure, firstTime );
      }
   }
}



touchSignal_t::touchSignal_t( void )
   : fd_( open( getTouchDev(), O_RDONLY ) )
{
   if( 0 <= fd_ ){
      int const pid_ = getpid();
      int const signum = nextRtSignal();
      fcntl(fd_, F_SETOWN, pid_);
      fcntl(fd_, F_SETSIG, signum );
      
      struct sigaction sa ;
   
      sa.sa_flags = SA_SIGINFO|SA_RESTART ;
      sa.sa_restorer = 0 ;
      sigemptyset( &sa.sa_mask );
      sa.sa_sigaction = touchHandler ;
      sigaddset( &sa.sa_mask, signum );
      sigaction(signum, &sa, 0 );
      
      int flags = fcntl( fd_, F_GETFL, 0 );
      fcntl( fd_, F_SETFL, flags | O_NONBLOCK | FASYNC );
   }
}

touchSignal_t::~touchSignal_t( void )
{
   if( 0 <= fd_ )
      close( fd_ );
}

#ifdef STANDALONE

#include <unistd.h>

int main( void )
{
   touchSignal_t &ts = touchSignal_t::get();

   pause();
   return 0 ;
}

#endif
                   
