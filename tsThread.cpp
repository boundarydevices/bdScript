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
 * Revision 1.9  2003-01-06 04:28:44  ericn
 * -made callbacks return bool (false if system shutting down)
 *
 * Revision 1.8  2003/01/05 01:55:34  ericn
 * -modified to have touch-screen thread close channel, added close() method
 *
 * Revision 1.7  2002/12/23 05:09:35  ericn
 * -modified to use either /dev/input/event0 or /dev/touchscreen/ucb1x00
 *
 * Revision 1.6  2002/12/15 00:00:22  ericn
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

#if TSINPUTAPI == 1
#define TSDEVICE "/dev/input/event0"
#else
#define TSDEVICE "/dev/touchscreen/ucb1x00"
#endif
static bool volatile shutdown = false ;

static void *tsThread( void *arg )
{
printf( "ts thread %p (id %x)\n", &arg, pthread_self() );   
   touchScreenThread_t *const obj = (touchScreenThread_t *)arg ;
//   struct input_event event ;
   ts_sample sample ;
   bool wasDown = false ;

   fbDevice_t &fb = getFB();
   unsigned const width  = fb.getWidth();
   unsigned const height = fb.getHeight();

   int numRead ;
   while( !shutdown && ( 0 <= ( numRead = ts_read( obj->tsDevice_, &sample, 1 ) ) ) )
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

         bool worked ;
         bool const down = ( 0 != sample.pressure );
         if( down != wasDown )
         {
            if( down )
               worked = obj->onTouch( sample.x, sample.y );
            else
               worked = obj->onRelease();

            wasDown = down ;
         }
         else if( down )
         {
            worked = obj->onMove( sample.x, sample.y );
         }
         if( !worked )
            break;
      } // translated value
   }
   
   ts_close( obj->tsDevice_ );
   
   fprintf( stderr, "touch screen thread shutting down due to error %m\n" );

   return 0 ;
}

touchScreenThread_t :: touchScreenThread_t( void )
   : threadHandle_( -1 ),
     tsDevice_( ts_open( TSDEVICE, 0 ) )
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
   close();
}

bool touchScreenThread_t :: onTouch
   ( unsigned        x, 
     unsigned        y )
{
   printf( "touch %u,%u\n", x, y );
   
   if( ( 10 >= x ) && ( 10 >= y ) )
      return *( bool volatile * )0 ;
   return true ;
}

bool touchScreenThread_t :: onRelease( void )
{
   printf( "release\n" );
   return true ;
}

bool touchScreenThread_t :: onMove
   ( unsigned        x, 
     unsigned        y )
{
   printf( "moveTo %u,%u\n", x, y );
   return true ;
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

void touchScreenThread_t :: close( void )
{
   shutdown = true ;
   if( isRunning() )
   {
      pthread_t thread = (pthread_t)threadHandle_ ;
      threadHandle_ = -1 ;

      pthread_cancel( thread );
      void *exitStat ;
      pthread_join( thread, &exitStat );
   }
}

#ifdef __MODULETEST__
#include <signal.h>
#include "hexDump.h"

static struct sigaction sa;
static struct sigaction oldint;

void handler(int sig) 
{
   pthread_t me = pthread_self();
   fprintf( stderr, "got signal, stack == %p (id %x)\n", &sig, me );
   fprintf( stderr, "sighandler at %p\n", handler );
   unsigned long addr = (unsigned long)&sig ;
   unsigned long page = addr & ~0xFFF ; // 4K
   unsigned long size = page+0x1000-addr ;
   
   hexDumper_t dumpStack( &sig, size );
   while( dumpStack.nextLine() )
      fprintf( stderr, "%s\n", dumpStack.getLine() );

   fflush( stderr );
   if( oldint.sa_handler )
      oldint.sa_handler( sig );

   exit( 1 );
}


int main( int argc, char const * const argv[] )
{
   // Initialize the sa structure
   sa.sa_handler = handler;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   
   // Set up the signal handler
   sigaction(SIGSEGV, &sa, NULL);

   printf( "main thread %p (id %x)\n", &argc, pthread_self() );
   
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

      printf( "shutting down thread\n" );

   }
   else
      fprintf( stderr, "Error opening touch device\n" );

   printf( "shutdown complete\n" );
   
   sleep( 10 );

   return 0 ;
}

#endif
