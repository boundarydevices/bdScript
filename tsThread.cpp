/*
 * Module tsThread.cpp
 *
 * This module defines the touchScreenThread_t
 * base class, which is used to wrap a thread that
 * reads from the touch screen.
 *
 *
 * Change History : 
 *
 * $Log: tsThread.cpp,v $
 * Revision 1.6  2002-12-15 00:00:22  ericn
 * -removed debug msg
 *
 * Revision 1.5  2002/11/30 17:32:58  ericn
 * -added support for move
 *
 * Revision 1.4  2002/11/29 16:41:54  ericn
 * -added return value from reader thread
 *
 * Revision 1.3  2002/11/21 14:08:13  ericn
 * -modified to clamp at display bounds
 *
 * Revision 1.2  2002/11/08 13:57:02  ericn
 * -modified to use tslib
 *
 * Revision 1.1  2002/11/03 15:39:36  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "tsThread.h"
#include <stdio.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <tslib.h>
#include "fbDev.h"

static void *tsThread( void *arg )
{
   touchScreenThread_t *const obj = (touchScreenThread_t *)arg ;
//   struct input_event event ;
   ts_sample sample ;
   bool wasDown = false ;

   fbDevice_t &fb = getFB();
   unsigned const width  = fb.getWidth();
   unsigned const height = fb.getHeight();

   int numRead ;
   while( 0 <= ( numRead = ts_read( obj->tsDevice_, &sample, 1 ) ) )
   {
      if( 1 == numRead )
      {
         //
         // normalize
         //
         if( 0 > sample.x )
            sample.x = 0 ;
         else if( width <= sample.x )
            sample.x = width - 1 ;
         if( 0 > sample.y )
            sample.y = 0 ; 
         else if( height <= sample.y )
            sample.y = height - 1 ;

         bool const down = ( 0 != sample.pressure );
         if( down != wasDown )
         {
            if( down )
               obj->onTouch( sample.x, sample.y );
            else
               obj->onRelease();

            wasDown = down ;
         }
         else if( down )
         {
            obj->onMove( sample.x, sample.y );
         }
      } // translated value
   }
   
   fprintf( stderr, "touch screen thread shutting down due to error %m\n" );

   return 0 ;
}

touchScreenThread_t :: touchScreenThread_t( void )
   : threadHandle_( -1 ),
     tsDevice_( ts_open( "/dev/input/event0", 0 ) )
//     fdDevice_( open( "/dev/input/event0", O_RDONLY ) )
{
   if( 0 != tsDevice_ )
   {
      if( 0 == ts_config( tsDevice_ ) ) 
      {
      }
      else
      {
         perror( "ts_config" );
         ts_close( tsDevice_ );
         tsDevice_ = 0 ;
      }
   }
   else
      fprintf( stderr, "Error opening touch screen device\n" );
}

touchScreenThread_t :: ~touchScreenThread_t( void )
{
   if( isOpen() )
   {
//      close( fdDevice_ );
//      fdDevice_ = -1 ;
      ts_close( tsDevice_ );
      tsDevice_ = 0 ;
   }

   if( isRunning() )
   {
      pthread_t thread = (pthread_t)threadHandle_ ;
      threadHandle_ = -1 ;

      pthread_cancel( thread );
      void *exitStat ;
      pthread_join( thread, &exitStat );
   }
}

void touchScreenThread_t :: onTouch
   ( unsigned        x, 
     unsigned        y )
{
   printf( "touch %u,%u\n", x, y );
}

void touchScreenThread_t :: onRelease( void )
{
   printf( "release\n" );
}

void touchScreenThread_t :: onMove
   ( unsigned        x, 
     unsigned        y )
{
}

bool touchScreenThread_t :: begin( void )
{
   pthread_t thread ;
   int create = pthread_create( &thread, 0, tsThread, this );
   if( 0 == create )
   {
      threadHandle_ = thread ;
      return true ;
   }
   else
      return false ;
}

#ifdef __MODULETEST__
int main( void )
{
   touchScreenThread_t thread ;
   if( thread.isOpen() )
   {
      if( thread.begin() )
      {
         printf( "started thread\n" );
         char inBuf[80];
         while( fgets( inBuf, sizeof( inBuf ), stdin ) )
            printf( ": " );
      }
      else
         fprintf( stderr, "Error starting thread\n" );
   }
   else
      fprintf( stderr, "Error opening touch device\n" );
   return 0 ;
}

#endif
