#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/gen-timer.h>
#include <poll.h>

int main( void )
{
   printf( "Hello, timerTest\n" );

   int fd = open( "/dev/timer", O_RDONLY );
   if( 0 <= fd )
   {
      printf( "timer device opened\n" );
      long long arg = 0 ;
      int result = ioctl( fd, GET_TSTAMP, &arg );
      if( 0 == result )
      {
         printf( "EXP: %lld\n", arg );
         arg = tickMs()+1000 ;
         result = ioctl( fd, SET_TSTAMP, &arg );
         if( 0 == result )
         {
            printf( "set timestamp to %lld successfully\n", arg );
            long long when ;
            result = read( fd, &when, sizeof( when ) );
            if( sizeof(when) == result )
            {
               printf( "read %u bytes, %lld, now %lld\n", result, when, tickMs() );
               pollfd filedes ;
               filedes.fd = fd ;
               filedes.events = POLLIN ;
               result = poll( &filedes, 1, 1000 );
               if( 0 < result )
               {
                  printf( "poll() ready at %lld\n", tickMs() );
                  arg = tickMs()+1000 ;
                  ioctl( fd, SET_TSTAMP, &arg );
                  result = poll( &filedes, 1, 1000 );
                  if( 0 < result )
                  {
                     printf( "poll() ready again at %lld\n", tickMs() );
                  }
                  else if( 0 == result )
                     fprintf( stderr, "poll() timeout\n" );
                  else
                     perror( "poll()" );
               }
               else if( 0 == result )
                  fprintf( stderr, "poll() timeout\n" );
               else
                  perror( "poll()" );
            }
            else
               perror( "read()" );
         }
         else
            perror( "ioctl(SET_TSTAMP)" );
      }
      else
         perror( "ioctl(GET_TSTAMP)" );

      close( fd );
   }
   else
      perror( "/dev/timer" );

   return 0 ;
}
