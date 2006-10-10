/*
 * rtsCTS.cpp
 *
 * This program will test the ability to get the state of the
 * CTS pin, toggle the RTS pin, and spew data over a serial port.
 * 
 */
 
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/poll.h>
#include <ctype.h>
#include "baudRate.h"
#include <stdlib.h>

static bool volatile doExit = false ;

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\r\n" );

   doExit = true ;
}

static void toggleRTS( int fdSerial )
{
   int line ;
   if( 0 == ioctl(fdSerial, TIOCMGET, &line) )
   {
      line ^= TIOCM_RTS ;
      if( 0 == ioctl(fdSerial, TIOCMSET, &line) )
      {
         printf( "toggled\r\n" );
      }
      else
         perror( "TIOCMGET" );
   }
   else
      perror( "TIOCMGET" );
}

static void setRaw( int fd, 
                    int baud, 
                    int databits, 
                    char parity, 
                    unsigned stopBits,
                    struct termios &oldState )
{
   tcgetattr(fd,&oldState);

   /* set raw mode for keyboard input */
   struct termios newState = oldState;
   newState.c_cc[VMIN] = 1;

   unsigned baudConst ;
   baudRateToConst( baud, baudConst );

   cfsetispeed(&newState, baudConst);
   cfsetospeed(&newState, baudConst);

   //
   // Note that this doesn't appear to work!
   // Reads always seem to be terminated at 16 chars!
   //
   newState.c_cc[VTIME] = 0; // 1/10th's of a second, see http://www.opengroup.org/onlinepubs/007908799/xbd/termios.html

   newState.c_cflag &= ~(PARENB|CSTOPB|CSIZE|CRTSCTS);              // Mask character size to 8 bits, no parity, Disable hardware flow control

   if( 'E' == parity )
   {
      newState.c_cflag |= PARENB ;
      newState.c_cflag &= ~PARODD ;
   }
   else if( 'O' == parity )
   {
      newState.c_cflag |= PARENB | PARODD ;
   }
   else {
   } // no parity... already set

   newState.c_cflag |= (CLOCAL | CREAD |CS8);                       // Select 8 data bits
   if( 7 == databits ){
      newState.c_cflag &= ~CS8 ;
   }

   if( 1 != stopBits )
      newState.c_cflag |= CSTOPB ;

   newState.c_lflag &= ~(ICANON | ECHO );                           // set raw mode for input
   newState.c_iflag &= ~(IXON | IXOFF | IXANY|INLCR|ICRNL|IUCLC);   //no software flow control
   newState.c_oflag &= ~OPOST;                      //raw output
   tcsetattr( fd, TCSANOW, &newState );
}

static void showStatus( int fd )
{
   int line ;
   if( 0 == ioctl(fd, TIOCMGET, &line) )
   {
      printf( "line status 0x%04x: ", line );
      if( line & TIOCM_CTS )
         printf( "CTS " );
      else
         printf( "~CTS " );
      if( line & TIOCM_RTS )
         printf( "RTS " );
      else
         printf( "~RTS " );
      printf( "\r\n" );
   }
   else
      perror( "TIOCMGET" );
}

static void waitModemChange( int fd )
{
   printf( "wait modem status..." ); fflush(stdout);
   int line = TIOCM_RNG | TIOCM_DSR | TIOCM_CTS | TIOCM_CD ;
   int result = ioctl(fd, TIOCMIWAIT, &line);
   printf( "\r\n" );
   if( 0 == result ){
      printf( "changed\r\n" );
      showStatus(fd);
   }
   else
      perror( "TIOCMWAIT" );
}

static char const spewOutput[] = {
   "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890\r\n"
};

int main( int argc, char const * const argv[] )
{
   if( 2 < argc )
   {
      char const *const deviceName = argv[1];
      int const fdSerial = open( deviceName, O_RDWR );
      if( 0 <= fdSerial )
      {
         fcntl( fdSerial, F_SETFD, FD_CLOEXEC );
         fcntl( fdSerial, F_SETFL, O_NONBLOCK );

	 int baud = strtoul( argv[2], 0, 0 );
         int databits = ( ( 3 < argc ) && ( 7 == strtoul( argv[3], 0, 0 ) ) )
                        ? 7
                        : 8 ;
         char parity = ( 4 < argc )
                       ? toupper( *argv[4] )
                       : 'N' ;
         unsigned stopBits = ( ( 5 < argc ) && ( '2' == *argv[5] ) )
                           ? 2 
                           : 1 ;

         printf( "device %s opened\n", deviceName );
         struct termios oldSerialState;
         setRaw( fdSerial, baud, databits, parity, stopBits, oldSerialState);

         struct termios oldStdinState;
         
         showStatus(fdSerial);
         
         signal( SIGINT, ctrlcHandler );
         pollfd fds[2]; 
         fds[0].fd = fdSerial ;
         fds[0].events = POLLIN | POLLERR ;
         fds[1].fd = fileno(stdin);
         fds[1].events = POLLIN | POLLERR ;

         while( !doExit )
         {
            int const numReady = ::poll( fds, 2, 1000 );
            if( 0 < numReady )
            {
               for( unsigned i = 0 ; i < 2 ; i++ )
               {
                  if( fds[i].revents & POLLIN )
                  {
                     char inBuf[80];
                     int numRead = read( fds[i].fd, inBuf, sizeof(inBuf) );
                     for( int j = 0 ; j < numRead ; j++ )
                     {
                        char const c = inBuf[j];
                        if( isprint(c) )
                           printf( "%c", c );
                        else
                           printf( "<%02x>", (unsigned char)c );
                     }
                     fflush(stdout);
                  }
               }
            }
         }

         tcsetattr( fdSerial, TCSANOW, &oldSerialState );
         
         close( fdSerial );
      }
      else
         perror( deviceName );
   }
   else
      fprintf( stderr, "Usage: setBaud /dev/ttyS0 baudRate\n" );
   return 0 ;
} 
