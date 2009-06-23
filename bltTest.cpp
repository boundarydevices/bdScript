/*
 * Program bltTest.cpp
 *
 * This test program will copy data to and from the screen
 * as rapidly as possible, counting the number of iterations
 * in 2 seconds.
 *
 *
 * Change History : 
 *
 * $Log: bltTest.cpp,v $
 * Revision 1.1  2006-06-14 13:54:38  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbDev.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "tickMs.h"

static bool volatile done = false ;

static void handler( int sig )
{
   done = true ;
}

static void blt( void *dest, void *src, unsigned rows, unsigned bytesPerRow )
{
   unsigned long const *srcLongs = (unsigned long const *)src ;
   unsigned long       *destLongs = (unsigned long *)dest ;

   unsigned long const cacheLinesPerRow = bytesPerRow / 32 ;

   __asm__ volatile (
      "  pld   [%0, #0]\n"
      "  pld   [%0, #32]\n"
      "  pld   [%1, #0]\n"
      "  pld   [%1, #32]\n"
      "  pld   [%0, #64]\n"
      "  pld   [%0, #96]\n"
      "  pld   [%1, #64]\n"
      "  pld   [%1, #96]\n"
      :
      : "r" (destLongs), "r" (srcLongs)
   );
   for( unsigned y = 0 ; y < rows ; y++ )
   {
      for( unsigned x = 0 ; x < cacheLinesPerRow ; x++ )
      {
         *destLongs++ = *srcLongs++ ;
         *destLongs++ = *srcLongs++ ;
         *destLongs++ = *srcLongs++ ;
         *destLongs++ = *srcLongs++ ;
         *destLongs++ = *srcLongs++ ;
         *destLongs++ = *srcLongs++ ;
         *destLongs++ = *srcLongs++ ;
         *destLongs++ = *srcLongs++ ;
         __asm__ volatile (
            "  pld   [%0, #96]\n"
            "  pld   [%1, #96]\n"
            :
            : "r" (destLongs), "r" (srcLongs)
         );
      }
   }
}

int main( int argc, char const * const argv[] )
{
   fbDevice_t &fb = getFB();
   
   void *const dataMem = malloc(fb.getMemSize());
   void *const dataMem2 = malloc(fb.getMemSize());
   
   signal (SIGALRM, handler);

   /*
    * Set the timer up to be non repeating, so that once it expires, it
    * doesn't start another cycle.  What you do depends on what you need
    * in a particular application.
    */

   struct itimerval     itimer;
   int                  interval = 2;   /* 1 second interval */
   
   itimer . it_interval . tv_sec = 0;
   itimer . it_interval . tv_usec = 0;
 
   /*
    * Set the time to expiration to interval seconds.
    * The timer resolution is milliseconds.
    */
   
   itimer . it_value . tv_sec = interval;
   itimer . it_value . tv_usec = 0;

   
   unsigned readCount = 0 ; 
   unsigned writeCount = 0 ; 
   unsigned long readTime = 0 ;
   unsigned long writeTime = 0 ;
   
   done = false ; setitimer (ITIMER_REAL, &itimer, NULL);
   while( !done )
   {
      long long start = tickMs();
      memcpy( dataMem, fb.getMem(), fb.getMemSize() );
      readTime += (unsigned long)(tickMs()-start);
      readCount++ ;
//      printf( "r" ); fflush(stdout);
      
      if( !done )
      {
         start = tickMs();
         memcpy( fb.getMem(), dataMem, fb.getMemSize() );
         writeTime += (unsigned long)(tickMs()-start);
         writeCount++ ;
//         printf( "w" ); fflush(stdout);
      }
   }
   
   printf( "\n%u/%u iterations in %u seconds system<->fb (memcpy)\n", readCount, writeCount, interval );
   printf( "  %lu ms read\n", readTime );
   printf( "  %lu ms write\n", writeTime );
   
   memcpy( dataMem2, dataMem, fb.getMemSize() );


   readCount = 0 ; 
   writeCount = 0 ; 
   readTime = 0 ;
   writeTime = 0 ;
   
   done = false ; setitimer (ITIMER_REAL, &itimer, NULL);
   while( !done )
   {
      long long start = tickMs();
      memcpy( dataMem, dataMem2, fb.getMemSize() );
      readTime += (unsigned long)(tickMs()-start);
      readCount++ ;
//      printf( "r" ); fflush(stdout);
      
      if( !done )
      {
         start = tickMs();
         memcpy( dataMem2, dataMem, fb.getMemSize() );
         writeTime += (unsigned long)(tickMs()-start);
         writeCount++ ;
//         printf( "w" ); fflush(stdout);
      }
   }

   printf( "\n%u/%u iterations in %u seconds system<->system (memcpy)\n", readCount, writeCount, interval );
   printf( "  %lu ms read\n", readTime );
   printf( "  %lu ms write\n", writeTime );

   /*
    * hand-crafted blt
    *
    */
   unsigned const stride = fb.getStride();
   readCount = 0 ; 
   writeCount = 0 ; 
   readTime = 0 ;
   writeTime = 0 ;
   
   done = false ; setitimer (ITIMER_REAL, &itimer, NULL);
   while( !done )
   {
      long long start = tickMs();
      blt( dataMem, fb.getMem(), fb.getHeight(), stride );
      readTime += (unsigned long)(tickMs()-start);
      readCount++ ;
//      printf( "r" ); fflush(stdout);
      
      if( !done )
      {
         start = tickMs();
         blt( fb.getMem(), dataMem, fb.getHeight(), stride );
         writeTime += (unsigned long)(tickMs()-start);
         writeCount++ ;
//         printf( "w" ); fflush(stdout);
      }
   }
   
   printf( "\n%u/%u iterations in %u seconds system<->fb (blt)\n", readCount, writeCount, interval );
   printf( "  %lu ms read\n", readTime );
   printf( "  %lu ms write\n", writeTime );
   
   readCount = 0 ; 
   writeCount = 0 ; 
   readTime = 0 ;
   writeTime = 0 ;
   
   done = false ; setitimer (ITIMER_REAL, &itimer, NULL);
   while( !done )
   {
      long long start = tickMs();
      blt( dataMem, dataMem2, fb.getHeight(), stride );
      readTime += (unsigned long)(tickMs()-start);
      readCount++ ;
//      printf( "r" ); fflush(stdout);
      
      if( !done )
      {
         start = tickMs();
         blt( dataMem2, dataMem, fb.getHeight(), stride );
         writeTime += (unsigned long)(tickMs()-start);
         writeCount++ ;
//         printf( "w" ); fflush(stdout);
      }
   }

   printf( "\n%u/%u iterations in %u seconds system<->system (blt)\n", readCount, writeCount, interval );
   printf( "  %lu ms read\n", readTime );
   printf( "  %lu ms write\n", writeTime );

   return 0 ;
}
