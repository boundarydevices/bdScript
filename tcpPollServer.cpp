/*
 * Module tcpPollServer.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: tcpPollServer.cpp,v $
 * Revision 1.1  2004-03-17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "tcpPollServer.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>
#include <stdio.h>

int main( int argc, char const * const argv[] )
{
   int const tcpFd = socket( AF_INET, SOCK_STREAM, 0 );
   if( 0 <= tcpFd )
   {
      int doit = 1 ;
      int result = setsockopt( tcpFd, SOL_SOCKET, SO_REUSEADDR, &doit, sizeof( doit ) );
      if( 0 != result )
         syslog( LOG_ERR, "SO_REUSEADDR:%d:%m\n", result );
      
      struct linger linger; /* allow a lingering, graceful close; */ 
      linger.l_onoff  = 0 ; 
      linger.l_linger = 0 ; 
      result = setsockopt( tcpFd, SOL_SOCKET, SO_LINGER, &linger, sizeof( linger ) );
      if( 0 != result )
         syslog( LOG_ERR, "SO_REUSEADDR:%d:%m\n", result );

      fcntl( tcpFd, F_SETFD, FD_CLOEXEC );
      
      sockaddr_in myAddress ;

      memset( &myAddress, 0, sizeof( myAddress ) );

      myAddress.sin_family      = AF_INET;
      myAddress.sin_addr.s_addr = 0 ; // local
      myAddress.sin_port        = 0x3232 ;

      if( 0 == bind( tcpFd, (struct sockaddr *) &myAddress, sizeof( myAddress ) ) )
      {
         if( 0 == listen( tcpFd, 20 ) )
         {
            pollfd fds[2]; 
            fds[0].fd = tcpFd ;
            fds[0].events = POLLIN | POLLERR ;
            fds[1].fd = -1 ;
            fds[2].events = 0 ;

            while( 1 )
            {
               int const numReady = ::poll( fds, 2, 1000 );
               if( 0 < numReady )
               {
                  printf( "ready\n" );
                  if( fds[0].revents )
                  {
                     printf( "main sock 0x%02x\n", fds[0].revents );
                     struct sockaddr_in clientAddr ;
                     socklen_t sockAddrSize = sizeof( clientAddr );
                     int newFd = ::accept( tcpFd, (struct sockaddr *)&clientAddr, &sockAddrSize );
                     if( 0 <= newFd )
                     {
                        if( 0 <= fds[1].fd )
                        {
                           printf( "closing fd %d\n", fds[1].fd );
                           close( fds[1].fd );
                        }
                        fds[1].fd = newFd ;
                        fds[1].events = POLLIN | POLLOUT | POLLERR | POLLHUP ;
                        printf( "accepted connection\n" );
                     }
                     else
                     {
                        syslog( LOG_ERR, "accept:%m\n" );
                        break;
                     }
                  }
                  if( fds[1].revents )
                  {
                     printf( "client sock 0x%02x\n", fds[1].revents );
                     fds[1].events &= ~fds[1].revents ;
                  }
               }
               else if( 0 == numReady )
                  fprintf( stderr, "idle\n" );
               else
               {
                  perror( "poll" );
                  break;
               }
            }
         }
         else
            syslog( LOG_ERR, ":listen %m\n" );
         close( tcpFd );
      }
      else
         syslog( LOG_ERR, ":bind %m\n" );
   }
   else
      syslog( LOG_ERR, ":socket:%m\n" );

   return 0 ;
}
