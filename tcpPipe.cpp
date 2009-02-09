/*
 * Program tcpPipe.cpp
 *
 * This program is designed to allow a client (usually jsExec) to 
 * establish a TCP connection asynchronously by spawning it as a 
 * child (pipe) process.
 *
 * It sends notifications of connection status to the parent process
 * on stderr:
 *
 *    [connected]
 *    [error] message goes here (printf %m)
 *    [disconnected]
 *
 * It will route any incoming data on stdin to the TCP connection if
 * connected and routes any input from the TCP socket to stdout.
 *
 * The program takes two command-line arguments:
 *
 *    serverip portNumber
 *
 * Change History : 
 *
 * $Log: tcpPipe.cpp,v $
 * Revision 1.2  2009-02-09 18:14:07  ericn
 * handle close more gracefully
 *
 * Revision 1.1  2008-12-13 00:31:25  ericn
 * added tcpPipe utility program
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
// #define DEBUGPRINT
#include "debugPrint.h"
#include <stdlib.h>
#include <stdarg.h>
#include <termios.h>
#include <sys/poll.h>
#include "tickMs.h"
#include <signal.h>

static void errMsg( char const *fmt, ... )
{
   enum {
      pageSize = 4096,
      maxMsgSize = pageSize - 16
   };
   char buf[maxMsgSize];

   va_list ap;
   va_start(ap,fmt);
   vsnprintf(buf,sizeof(buf),fmt,ap);
   va_end(ap);
   fprintf( stderr, "[error] %s\n", buf );
}

void handle_signal( int sig )
{
}

int main( int argc, char const * const argv[] )
{
   if( 3 == argc ){
      sockaddr_in serverAddr ;
      memset( &serverAddr, 0, sizeof( serverAddr ) );
      serverAddr.sin_family = AF_INET;
      if( 0 != inet_aton( argv[1], &serverAddr.sin_addr ) )
      {
         serverAddr.sin_port = ntohs(strtoul(argv[2], 0, 0) );

         int tcpSock = socket( AF_INET, SOCK_STREAM, 0 );
         if( 0 <= tcpSock ){
            fcntl( tcpSock, F_SETFD, FD_CLOEXEC );
            debugPrint( "%s: connect to 0x%lx, port 0x%x", argv[0], serverAddr.sin_addr.s_addr, serverAddr.sin_port );

            struct sigaction act1 ;
            act1.sa_handler = handle_signal;
            sigemptyset(&act1.sa_mask);
            act1.sa_flags = 0;
            act1.sa_flags |= SA_INTERRUPT;
            sigaction(SIGALRM, &act1, 0);
            sigaction(SIGINT, &act1, 0);
            
            bool connected = false ;
            unsigned connectFailures = 0 ;
            bool bail = false ;

            enum fd_e {
               FDSTDIN,    // note dual-purpose (0 is fileno(stdin))
               FDSOCK,
               NUMFDS
            };

            struct termios oldState ;
            tcgetattr( FDSTDIN, &oldState );
            struct termios raw = oldState ;
            raw.c_lflag &=~ (ICANON | ECHO );
            tcsetattr( FDSTDIN, TCSANOW, &raw );
            fcntl(FDSTDIN, F_SETFL, fcntl(FDSTDIN, F_GETFL) | O_NONBLOCK);

            while( !bail ){
               while( !connected && !bail ){
                  alarm(1+10*connectFailures);
                  int const connectResult = connect( tcpSock, (struct sockaddr *) &serverAddr, sizeof( serverAddr ) );
                  if( 0 == connectResult ){
                     alarm(0);
                     fprintf( stderr, "[[connected]]\n", errno );
                     connected = true ;
                  } else {
                     errMsg( "connect:%m" );
                     ++connectFailures ;
                     char inBuf[512];
                     int numRead ;
                     while(0 < (numRead = read(FDSTDIN,inBuf,sizeof(inBuf)))){
                        if( '\x03' == inBuf[0] ){
                           bail = true ;
                           break;
                        }
                     }
                     if( bail )
                        break;
                     sleep(10*connectFailures);
                  }
               }

               if( bail )
                  break;

               pollfd fds_[3];
               fds_[FDSTDIN].fd = 0 ;
               fds_[FDSTDIN].events = POLLIN|POLLHUP|POLLERR ;
               fds_[FDSOCK].fd = tcpSock ;
               fds_[FDSOCK].events = POLLIN|POLLERR|POLLHUP ;

               fcntl(tcpSock, F_SETFL, fcntl(tcpSock, F_GETFL) | O_NONBLOCK);
int prev0 = -1 ;
int prev1 = -1 ;
int prevReady = -1 ;

               while( connected && !bail ){
                  int const numFdsReady = poll(fds_,NUMFDS,1000);
                  
if( ( prev0 != fds_[0].revents )
    ||
    ( prev1 != fds_[1].revents )
    ||
    ( prevReady != numFdsReady ) ){
   debugPrint( "%d fds ready: 0x%04x 0x%04x\n", numFdsReady, fds_[0].revents, fds_[1].revents );
   prev0 = fds_[0].revents ;
   prev1 = fds_[1].revents ;
   prevReady = numFdsReady ;
}

                  if( fds_[FDSOCK].revents & (POLLERR|POLLHUP) ){
                     errMsg( "socket error: %m" );
                     close( tcpSock );
                     connected = false ;

                     tcpSock = socket( AF_INET, SOCK_STREAM, 0 );
                     if( 0 <= tcpSock ){
                        fcntl( tcpSock, F_SETFD, FD_CLOEXEC );
                        fcntl( tcpSock, F_SETFL, O_NONBLOCK );
                        fds_[FDSOCK].fd = tcpSock ;
                     }
                     else {
                        errMsg( "socket error %m" );
                        bail = true ;
                     }
                     break;
                  }

                  if( connected && (fds_[FDSOCK].revents & POLLIN) ){
                     char inBuf[512];
                     int numRead ;
                     do {
                        numRead = read(tcpSock,inBuf,sizeof(inBuf));
                        if( 0 < numRead ){
                           unsigned totalWritten = 0 ;
                           while( totalWritten < numRead ){
                              int const numWritten = write( 1, inBuf+totalWritten, numRead-totalWritten );
                              if( 0 <= numWritten ){
                                 totalWritten += numWritten ;
                              } else if( 0 > numWritten ){
                                 errMsg( "write(stdout):%m" );
                                 connected = false ;
                                 break;
                              }
                           }
                        }
                        else if( (-1 == numRead) && (EAGAIN == errno) ){
                           ; // wait for more
                        }
                        else {
                           errMsg( "read(tcp):%d:%d:%m", numRead, errno );
                           int err = 0 ;
                           socklen_t errSize = sizeof(err);
                           getsockopt( tcpSock, SOL_SOCKET, SO_ERROR, (void *)&err, &errSize);
                           errMsg( "sockerr:%d:%d:%m", err, errno );
                           connected = false ;
                        }
                     } while( 0 < numRead );
                  }

                  if( fds_[FDSTDIN].revents & POLLIN ){
                     char inBuf[4096];
                     int numRead = read(FDSTDIN,inBuf,sizeof(inBuf));
                     if( 0 < numRead ){
                        debugPrint( "stdin: %d bytes\n", numRead );
                        if( '\x03' == inBuf[0] ){
                           bail = true ;
                        }
                        if( connected ){
                           int numWritten = write(tcpSock,inBuf,numRead);
                           if( numWritten != numRead ){
                              errMsg( "write(sock):%m" );
                           }
                        }
                     }
                     else if( 0 > numRead )
                        errMsg( "read(stdin):%m" );
                     else{
                        errMsg( "read(stdin):0 bytes:%m" );
                        fds_[FDSTDIN].events &= ~POLLIN ;
                        bail = true ;
                     }
                  }
               } // while( connected )

               if( bail )
                  break;

               fprintf( stderr, "[[disconnected]]\n", errno );

               fcntl(tcpSock, F_SETFL, fcntl(tcpSock, F_GETFL) & ~O_NONBLOCK);
               close(tcpSock);
               tcpSock = socket( AF_INET, SOCK_STREAM, 0 );
               if( 0 > tcpSock ){
                  errMsg( "socket error: %m" );
                  bail = true ;
               }
            }
            tcsetattr( 0, TCSANOW, &oldState );
         }
         else
            errMsg( "Error creating socket: %m" );
      }
      else
         errMsg( "Invalid ip %s", argv[1] );
   }
   else
      errMsg( "Usage: %s serverip portNum", argv[0] );
   return 0 ;
}
