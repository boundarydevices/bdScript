
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>

void handler( int sig )
{
   fprintf( stderr, "got signal %u\n", sig );
   fprintf( stderr, "sighandler at %p\n", handler );

   void *btArray[128];
   int btSize = backtrace(btArray, sizeof(btArray) / sizeof(void *) );
   
   fprintf( stderr, "########## Backtrace ##########\n"
                    "Number of elements in backtrace: %u\n", btSize );

   if (btSize > 0)
   {
      char** btSymbols = backtrace_symbols( btArray, btSize );
      if( btSymbols )
      {
         for (int i = btSize - 1; i >= 0; --i)
            fprintf( stderr, "%s\n", btSymbols[i] );
      }
   }

   fprintf( stderr, "Handler done.\n" );
   exit( 0 );
}

static void f();

int main(int argc, char **argv)
{
   struct itimerval     itimer;
   int                  interval = 1;   /* 1 second interval */

   /*
    * Set up a signal handler for the SIGALRM signal which is what the
    * expiring timer will set.
    */

   signal (SIGALRM, handler);

   /*
    * Set the timer up to be non repeating, so that once it expires, it
    * doesn't start another cycle.  What you do depends on what you need
    * in a particular application.
    */

   itimer . it_interval . tv_sec = 0;
   itimer . it_interval . tv_usec = 0;
 
   /*
    * Set the time to expiration to interval seconds.
    * The timer resolution is milliseconds.
    */
   
   itimer . it_value . tv_sec = interval;
   itimer . it_value . tv_usec = 0;

   if (setitimer (ITIMER_REAL, &itimer, NULL) < 0)
   {
      perror ("StartTimer could not setitimer");
      exit (0);
   }

   f();

   return 0 ;
}


static void h()
{
   while( 1 )
      ;
}

static void g()
{
   h();
}

static void f()
{
   g();
}

