/*
 * Module touchPoll.cpp
 *
 * This module defines the methods of the touchPoll_t
 * class as declared in touchPoll.h
 *
 *
 * Change History : 
 *
 * $Log: touchPoll.cpp,v $
 * Revision 1.7  2004-12-03 04:43:50  ericn
 * -use median/mean filters
 *
 * Revision 1.6  2004/11/26 15:28:54  ericn
 * -use median/mean instead of range filter
 *
 * Revision 1.5  2004/01/01 20:11:42  ericn
 * -added isOpen() routine, and switched pollHandlers to use close()
 *
 * Revision 1.4  2003/11/28 14:04:48  ericn
 * -removed debug msgs
 *
 * Revision 1.3  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.2  2003/11/24 19:09:13  ericn
 * -added deglitch
 *
 * Revision 1.1  2003/10/05 19:15:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "touchPoll.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


// #define DEBUGPRINT
#include "debugPrint.h"
#include "rollingMedian.h"
#include "rollingMean.h"

#define MEDIANRANGE 5
static rollingMedian_t medianX_( MEDIANRANGE );
static rollingMedian_t medianY_( MEDIANRANGE );

#define MEANRANGE 5
static rollingMean_t meanX_( MEANRANGE );
static rollingMean_t meanY_( MEANRANGE );

struct ts_event  {   /* Used in UCB1x00 style touchscreens (the default) */
	unsigned short pressure;
	unsigned short x;
	unsigned short y;
	unsigned short pad;
	struct timeval stamp;
};

touchPoll_t :: touchPoll_t
   ( pollHandlerSet_t &set,
     char const       *devName )
   : pollHandler_t( open( devName, O_RDONLY ), set )
{
   if( isOpen() )
   {
      fcntl( fd_, F_SETFD, FD_CLOEXEC );
      fcntl( fd_, F_SETFL, O_NONBLOCK );
      setMask( POLLIN );
      set.add( *this );
   }
}

touchPoll_t :: ~touchPoll_t( void )
{
   if( isOpen() )
      close();
}

void touchPoll_t :: onTouch( int x, int y, unsigned pressure, timeval const &tv )
{
   debugPrint( "touch screen touch : %u/%u, pressure %u\n", x, y, pressure );
}

void touchPoll_t :: onRelease( timeval const &tv )
{
   debugPrint( "touch screen release\n" );
}

static bool wasDown = false ;

void touchPoll_t :: onDataAvail( void )
{
   ts_event nextEvent ;
   ts_event event ;
   unsigned count = 0 ;
   int numRead ;

   while( sizeof( nextEvent ) == ( numRead = read( fd_, &nextEvent, sizeof( nextEvent ) ) ) )
   {
      medianX_.feed( nextEvent.x );
      medianY_.feed( nextEvent.y );

debugPrint( "sample: %u/%u\n", nextEvent.x, nextEvent.y );
#ifdef XDEBUGPRINT
      printf( "medianX: " ); medianX_.dump();
      printf( "medianY: " ); medianY_.dump();
#endif

      unsigned short sampleX ;
      if( medianX_.read( sampleX ) )
         meanX_.feed( sampleX );
      unsigned short sampleY ;
      if( medianY_.read( sampleY ) )
         meanY_.feed( sampleY );
      event = nextEvent ;
      count++ ;
   }
   
   if( 0 < count )
   {
      if( 0 < event.pressure )
      {
         unsigned short x, y ;
//         x = event.x ; y = event.y ; if( 1 )
//         if( medianX_.read( x ) && medianY_.read( y ) )
         if( meanX_.read( x ) && meanY_.read( y ) )
         {
debugPrint( "out: %u/%u\n", x, y );
            onTouch( x, y, event.pressure, event.stamp );
            if( !wasDown )
            {
debugPrint( "first touch at %u:%u\n", x, y );
               wasDown = true ;
            }
         }
         else
         {
debugPrint( "not enough values\n" );
         }
      }
      else
      {
         if( wasDown )
         {
            onRelease( event.stamp );
            wasDown = false ;
         }
         else
         {
debugPrint( "missed touch\n" );
         }
         medianX_.reset(); medianY_.reset();
         meanX_.reset(); meanY_.reset();
      }
   }
}

#ifdef STANDALONE

int main( void )
{
   pollHandlerSet_t handlers ;
   touchPoll_t      touchPoll( handlers );
   if( touchPoll.isOpen() )
   {
      debugPrint( "opened touchPoll: fd %d, mask %x\n", touchPoll.getFd(), touchPoll.getMask() );

      int iterations = 0 ;
      while( 1 )
      {
         handlers.poll( -1 );
//         debugPrint( "poll %d\n", ++iterations );
      }
   }
   else
      perror( "/dev/touchscreen/ucb1x00" );

   return 0 ;
}

#endif
