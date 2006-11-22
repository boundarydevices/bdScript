/*
 * Module udpPoll.cpp
 *
 * This module defines the methods of the udpPoll_t
 * class as declared in udpPoll.h
 *
 *
 * Change History : 
 *
 * $Log: udpPoll.cpp,v $
 * Revision 1.3  2006-11-22 17:15:45  ericn
 * -write msgs to stdout in base (abstract) onMsg() routine
 *
 * Revision 1.2  2004/01/01 20:11:42  ericn
 * -added isOpen() routine, and switched pollHandlers to use close()
 *
 * Revision 1.1  2003/12/28 20:56:07  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "udpPoll.h"
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

udpPoll_t :: udpPoll_t
   ( pollHandlerSet_t &set,
     unsigned short    port )
   : pollHandler_t( socket( AF_INET, SOCK_DGRAM, 0 ), set ),
     port_( port )
{
   sockaddr_in local ;
   local.sin_family      = AF_INET ;
   local.sin_addr.s_addr = INADDR_ANY ;
   local.sin_port        = port ;
   bind( getFd(), (struct sockaddr *)&local, sizeof( local ) );
   fcntl( getFd(), F_SETFD, FD_CLOEXEC );
   setMask( POLLIN );
   set.add( *this );
   
}

udpPoll_t :: ~udpPoll_t( void )
{
   if( isOpen() )
      close();
}

void udpPoll_t :: onMsg
   ( void const        *msg,
     unsigned           msgLen,
     sockaddr_in const &sender )
{
   printf( "received %u bytes from %s/%u\n", msgLen, inet_ntoa( sender.sin_addr ), ntohs( sender.sin_port ) );
   fwrite( msg, msgLen, 1, stdout );
   printf( "\n" );
}

void udpPoll_t :: onDataAvail( void )
{
   int numAvail = 0 ;
   if( 0 == ioctl( getFd(), SIOCINQ, &numAvail ) )
   {
      if( 0 < numAvail )
      {
         char *data = (char *)malloc( numAvail );
         if( data )
         {
            sockaddr_in fromAddr ;
            socklen_t   fromSize = sizeof( fromAddr );
            int numRead = recvfrom( getFd(), 
                                    (char *)data, numAvail, MSG_DONTWAIT,
                                    (struct sockaddr *)&fromAddr, 
                                    &fromSize );
            if( 0 < numRead )
            {
               onMsg( data, numRead, fromAddr );
            }

            free( data );
         }
         else
            perror( "udp:malloc" );
      }
   }
   else
      perror( "SIOCINQ" );
}

#ifdef STANDALONE

int main( void )
{
   pollHandlerSet_t handlers ;
   udpPoll_t udp( handlers, 0x2020 );

   while( 1 )
   {
      handlers.poll( -1 );
   }

   printf( "completed\n" ); fflush( stdout );

   return 0 ;
}

#endif
