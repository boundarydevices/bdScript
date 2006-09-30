#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/serial.h>

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc ){
      int fd = open( argv[1], O_RDONLY );
      if( 0 <= fd ){
         printf( "opened..fd %d\n", fd );
         struct serial_icounter_struct counter ;
         if( 0 == ioctl( fd, TIOCGICOUNT, &counter ) ){
            printf( "read stats\n" );
            printf( "%5d   cts\n", counter.cts );
            printf( "%5d   dsr\n", counter.dsr );
            printf( "%5d   rng\n", counter.rng );
            printf( "%5d   dcd\n", counter.dcd );
            printf( "%5d   rx\n", counter.rx );
            printf( "%5d   tx\n", counter.tx );
            printf( "%5d   frame\n", counter.frame );
            printf( "%5d   overrun\n", counter.overrun );
            printf( "%5d   parity\n", counter.parity );
            printf( "%5d   brk\n", counter.brk );
            printf( "%5d   buf_overrun\n", counter.buf_overrun );
         }
         else
            perror( "TIOCGICOUNT" );
         close( fd );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: serialCounts /dev/ttyS1\n" );
   return 0 ;
}

