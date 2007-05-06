/*
 * miniterm.cpp
 *
 * This program is a miniature terminal emulator, connecting the 
 * input of a device to stdout and stdin to the output of the device.
 * 
 * It also displays unprintable incoming values in hex.
 * 
 * Usage is:
 *		miniterm /dev/myDevice
 *
 * <Ctrl-C> shuts the program down.
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
#include "setSerial.h"

static bool volatile doExit = false ;

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      char const *const deviceName = argv[1];
      int const fdDevice = open( deviceName, O_RDWR );
      if( 0 <= fdDevice )
      {
         printf( "device %s opened\n", deviceName );
         
         fcntl( fdDevice, F_SETFD, FD_CLOEXEC );
         fcntl( fdDevice, F_SETFL, O_NONBLOCK );
         
         struct termios oldSerialState;
         tcgetattr(0,&oldSerialState);

         setRaw( 0 );
         
         pollfd fds[2]; 
         fds[0].fd = fdDevice ;
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
                     int const fdIn = fds[i].fd ;
                     int numRead = read( fdIn, inBuf, sizeof(inBuf) );
                     int const fdOut = fds[i^1].fd ;

                     if( fdIn == fdDevice ){
                        for( int j = 0 ; j < numRead ; j++ )
                        {
                           char const c = inBuf[j];
                           if( isprint(c) 
                               ||
                               ( '\r' == c ) 
                               ||
                               ( '\n' == c ) )
                              write( fileno(stdout), &c, 1 );
                           else
                              printf( "<%02x>", (unsigned char)c );
                        }
                        fflush(stdout);
                     }
                     else {
                        char const *nextIn = inBuf ;
                        for( unsigned i = 0 ; i < numRead ; i++ ){
                           if( '\x03' == inBuf[i] ){
                              doExit = true ;
                              printf( "<ctrl-c>\r\n" );
                              break ;
                           }
                        }
                        while( !doExit && ( 0 < numRead ) ){
                           int numWritten = write( fdOut, nextIn, numRead );
                           if( 0 <= numWritten ){
                              nextIn += numWritten ;
                              numRead -= numWritten ;
                           }
                           else
                              break ;
                        }
                     } // read from keyboard. send to device
                  }
               }
            }
         }

         tcsetattr( 0, TCSANOW, &oldSerialState );
         
         close( fdDevice );
      }
      else
         perror( deviceName );
   }
   else
      fprintf( stderr, "Usage: miniterm /dev/myDevice\n" );
   return 0 ;
} 
