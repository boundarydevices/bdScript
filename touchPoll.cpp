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
 * Revision 1.2  2003-11-24 19:09:13  ericn
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
   printf( "touch screen touch : %u/%u, pressure %u\n", x, y, pressure );
}

void touchPoll_t :: onRelease( timeval const &tv )
{
   printf( "touch screen release\n" );
}

void touchPoll_t :: onDataAvail( void )
{
   ts_event prevEvent ;
   ts_event event ;
   unsigned count = 0 ;
   int numRead ;

   //
   // prevX and prevY are used to de-glitch
   //
   int prevX = prevX_ ;
   int prevY = prevY_ ;

   while( sizeof( event ) == ( numRead = read( fd_, &event, sizeof( event ) ) ) )
   {
      if( 0 < count )
      {
         prevX = prevEvent.x ;
         prevY = prevEvent.y ;
      }
      prevEvent = event ;
      count++ ;
   }
   
   if( 0 < count )
   {
      if( 0 < prevEvent.pressure )
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

         if( ( prevEvent.x >= minX ) && ( prevEvent.x <= maxX )
             &&
             ( prevEvent.y >= minY ) && ( prevEvent.y <= maxY ) )
         {
            onTouch( prevEvent.x, prevEvent.y, prevEvent.pressure, prevEvent.stamp );
         }
         else
         {
//            printf( "x: range[%d,%d], value %d\n", minX, maxX, prevEvent.x );
//            printf( "y: range[%d,%d], value %d\n", minY, maxY, prevEvent.y );
//            printf( "e" ); 
//            fflush( stdout );
         } // eat wild one
         
         xMotion_ = prevEvent.x - prevX_ ;
         yMotion_ = prevEvent.y - prevY_ ;
         prevX_ = prevEvent.x ;
         prevY_ = prevEvent.y ;
      }
      else
      {
         onRelease( prevEvent.stamp );
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
      printf( "opened touchPoll: fd %d, mask %x\n", touchPoll.getFd(), touchPoll.getMask() );

      int iterations = 0 ;
      while( 1 )
      {
         handlers.poll( -1 );
//         printf( "poll %d\n", ++iterations );
      }
   }
   else
      perror( "/dev/touchscreen/ucb1x00" );

   return 0 ;
}

#endif
