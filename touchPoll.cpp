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
 * Revision 1.4  2003-11-28 14:04:48  ericn
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
#include "debugPrint.h"

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
   , prevX_( -1 )
   , xRange_( 0 )
   , prevY_( -1 )
   , xMotion_( 0 )
   , yRange_( 0 )
   , yMotion_( 0 )
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
   {
      close( fd_ );
      fd_ = -1 ;
   }
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
   ts_event event ;
   ts_event nextEvent ;
   unsigned count = 0 ;
   int numRead ;

   //
   // prevX and prevY are used to de-glitch
   //
   int prevX = prevX_ ;
   int prevY = prevY_ ;

   while( sizeof( nextEvent ) == ( numRead = read( fd_, &nextEvent, sizeof( nextEvent ) ) ) )
   {
      if( 0 < count )
      {
         prevX = event.x ;
         prevY = event.y ;
      }
      event = nextEvent ;
      count++ ;
   }
   
   if( 0 < count )
   {
      if( 0 < event.pressure )
      {
         int minX, maxX ;
         if( 0 != xRange_ )
         {
            minX = prevX - xRange_ ;
            maxX = prevX + xRange_ ;
            if( 0 <= xMotion_ )
               maxX += xMotion_ ; // allow extra in same direction
            else
               minX -= xMotion_ ;
         }
         else
         {
            minX = 0x80000000 ;
            maxX = 0x7fffffff ;
         }

         int minY, maxY ;
         if( 0 != yRange_ )
         {
            minY = prevY - yRange_ ;
            maxY = prevY + yRange_ ;
            if( 0 <= yMotion_ )
               maxY += yMotion_ ; // allow extra in same direction
            else
               minY -= yMotion_ ;
         }
         else
         {
            minY = 0x80000000 ;
            maxY = 0x7fffffff ;
         }

         if( ( event.x >= minX ) && ( event.x <= maxX )
             &&
             ( event.y >= minY ) && ( event.y <= maxY ) )
         {
            onTouch( event.x, event.y, event.pressure, event.stamp );
         }
         else
         {
//            debugPrint( "x: range[%d,%d], value %d\n", minX, maxX, event.x );
//            debugPrint( "y: range[%d,%d], value %d\n", minY, maxY, event.y );
//            debugPrint( "e" ); 
//            fflush( stdout );
         } // eat wild one
         
         xMotion_ = event.x - prevX_ ;
         yMotion_ = event.y - prevY_ ;
         prevX_ = event.x ;
         prevY_ = event.y ;
         if( !wasDown )
         {
debugPrint( "first touch at %u:%u\n", event.x, event.y );
            wasDown = true ;
         }
      }
      else
      {
debugPrint( "release at %u/%u\n", prevX_, prevY_ );
         xMotion_ = yMotion_ = 0 ;
         prevX_ = prevY_ = 0 ;
         onRelease( event.stamp );
         wasDown = false ;
      }
   }
}

void touchPoll_t :: setRange( unsigned x, unsigned y )
{
   xRange_ = x ;
   yRange_ = y ;
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
