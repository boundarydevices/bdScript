/*
 * Program udpEcho.cpp
 *
 * This program opens a UDP socket and echos anything 
 * received to the sender.
 *
 *
 * Change History : 
 *
 * $Log: udpEcho.cpp,v $
 * Revision 1.1  2004-11-21 16:36:09  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _MSC_VER
#else
#define CLOSESOCK close
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <sys/select.h>

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      char *end ;
      unsigned long portNum = strtoul( argv[1], &end, 0 );
      if( ( 0 < portNum ) && ( 0x10000 > portNum ) )
      {
         int const sockFd = socket( AF_INET, SOCK_DGRAM, 0 );
         if( 0 <= sockFd )
         {
            sockaddr_in local ;
            local.sin_family      = AF_INET ;
            local.sin_addr.s_addr = INADDR_ANY ;
            local.sin_port        = htons( (unsigned short)portNum );
            if( 0 == bind( sockFd, (struct sockaddr *)&local, sizeof( local ) ) )
            {
               printf( "bound to port 0x%lx\n", portNum );
               fd_set readFds ;
          
               while( 1 )
               {
                  FD_ZERO( &readFds );
                  FD_SET( sockFd, &readFds );
           
                  int const num = select( sockFd+1, &readFds, 0, 0, 0 );
                  if( 0 < num )
                  {
                     char data[512];
                     sockaddr_in fromAddr ;
                     socklen_t   fromSize = sizeof( fromAddr );
                     int numRead = recvfrom( sockFd, 
                                             data, sizeof(data), 0,
                                             (struct sockaddr *)&fromAddr, 
                                             &fromSize );
                     if( 0 < numRead )
                     {
                        printf( "rx %d bytes\n", numRead );
                        int const numSent = sendto( sockFd, data, numRead, 0, 
                                                    (struct sockaddr *)&fromAddr, fromSize );
                        if( numSent == numRead )
                           printf( "sent %d bytes\n", numSent );
                        else
                           perror( "sendto" );
                     }
                     else
                        fprintf( stderr, "recvfrom:%d:%m\n", numRead );
                  }
                  else
                  {
                     perror( "select" );
                     break;
                  }
               } // until stopped
            }
            else
               perror( "bind" );
            
            CLOSESOCK( sockFd );
         }
         else
            perror( "socket" );
      }
      else
         fprintf( stderr, "Invalid port %s\n", argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s portNum\n", argv[0] );
   return 0 ;
}

