#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#include <sys/poll.h>

static int tio_get_rs232_lines( int fd )
{
   int flags;
   if ( ioctl(fd, TIOCMGET, &flags ) < 0 )
      printf( "tio_get_rs232_lines: TIOCMGET failed\n" );
   else
   {
      printf( "tio flags %08x\n", flags );
      if ( flags & TIOCM_RTS ) puts( " RTS" );
      if ( flags & TIOCM_CTS ) puts( " CTS" );
      if ( flags & TIOCM_DSR ) puts( " DSR" );
      if ( flags & TIOCM_DTR ) puts( " DTR" );
      if ( flags & TIOCM_CD )  puts( " DCD" );
      if ( flags & TIOCM_RI  ) puts( " RI" );
   }
   return flags;
}

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      int const fd = open( argv[1], O_RDWR | O_NOCTTY );
      if( 0 <= fd )
      {
         int flags = tio_get_rs232_lines( fd );

         if ( ioctl(fd, TIOCMSET, &flags ) < 0 )
            printf( "TIOCMSET failed\n" );

         while( 1 )
         {
            pollfd rdPoll ;
            rdPoll.fd = fd ;
            rdPoll.events = POLLIN | POLLERR ;
            int const numEvents = poll( &rdPoll, 1, 1000 );
            if( 1 == numEvents )
            {
               tio_get_rs232_lines( fd );
               if( 0 != ( POLLIN & rdPoll.events ) )
               {
                  char inBuf[256];
                  int numRead = read( fd, inBuf, sizeof( inBuf ) );
                  if( 0 < numRead )
                  {
                     write( 1, inBuf, numRead );
                  }
                  else
                  {
                     fprintf( stderr, "[EOF]\n" );
                     break; // eof
                  }
               }
               else if( 0 != ( POLLERR & rdPoll.events ) )
               {
                  perror( "pollErr" );
                  break;
               }
            }
            else
            {
               perror( "timeout" );
            }
            flags = tio_get_rs232_lines( fd );
            flags = ~flags ;
            if ( ioctl(fd, TIOCMSET, &flags ) < 0 )
            {
               printf( "TIOCMSET failed\n" );
               break;
            }
         }

         close( fd );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "usage : %s /dev/port\n", argv[0] );
   
   return 0 ;
}


