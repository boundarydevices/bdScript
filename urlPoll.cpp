/*
 * Module urlPoll.cpp
 *
 * This module defines the methods of the urlPoll_t
 * class as declared in urlPoll.h
 *
 *
 * Change History : 
 *
 * $Log: urlPoll.cpp,v $
 * Revision 1.1  2004-02-01 09:53:18  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "urlPoll.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/ioctl.h>

urlPoll_t :: urlPoll_t
   ( unsigned long     serverIP,
     unsigned short    serverPort,
     pollHandlerSet_t &set )
   : pollHandler_t( socket( AF_INET, SOCK_STREAM, 0 ), set )
   , serverIP_( serverIP )
   , serverPort_( serverPort )
   , state_( closed )
{
   if( 0 < getFd() )
   {
      fcntl( getFd(), F_SETFD, FD_CLOEXEC );
      fcntl( getFd(), F_SETFL, O_NONBLOCK );
      
      sockaddr_in serverAddr ;
      memset( &serverAddr, 0, sizeof( serverAddr ) );
      
      serverAddr.sin_family      = AF_INET;
      serverAddr.sin_addr.s_addr = serverIP ;
      serverAddr.sin_port        = serverPort ;
   
      int const connectResult = connect( getFd(), (struct sockaddr *) &serverAddr, sizeof( serverAddr ) );
printf( "connectResult:%d:%m\n", connectResult );
      if( ( 0 == connectResult )
          ||
          ( ( -1 == connectResult ) && ( EINPROGRESS == errno ) ) )
      {
         int doit = 0xFFFFFFFF ;
         setsockopt( getFd(), SOL_SOCKET, TCP_NODELAY, &doit, sizeof( doit ) );
         state_ = connecting ;
         setMask( POLLIN|POLLOUT|POLLERR|POLLHUP );
         set.add( *this );
      }
      else
      {
         perror( "connect" );
         close();
      }
   }
}

urlPoll_t :: ~urlPoll_t( void )
{
   parent_.removeMe( *this );
   close();
}

void urlPoll_t :: onDataAvail( void )     // POLLIN
{
   printf( "dataAvail\n" );
   checkStatus();
   if( connected == getState() )
   {
      int numAvail ;
      int const ioctlResult = ioctl( getFd(), FIONREAD, &numAvail );
      if( ( 0 == ioctlResult )
          &&
          ( 0 < numAvail ) )
      {
printf( "%u bytes\n", numAvail );
   
         char inData[2048];
         if( sizeof( inData ) < numAvail )
            numAvail = sizeof( inData );
         int const numRead = read( getFd(), inData, numAvail );
         if( 0 < numRead )
         {
            write( fileno( stdout ), inData, numRead );
            fflush( stdout );
         }
         return ;
      }
      else if( 0 == ioctlResult )
      {
printf( "?? closed ??\n" );
         onHUP();
      }
      else
         perror( "FIONREAD" );
   }
   setMask( getMask() & ~POLLIN );
}

void urlPoll_t :: onWriteSpace( void )    // POLLOUT
{
   printf( "WriteSpace\n" );
   checkStatus();
   if( connected == state_ )
   {
      char const getMsg[] = {
         "GET http://ericdesk/ HTTP/1.1\r\n"
         "Host: ericdesk\r\n"
         "Accept: */*\r\n"
         "Accept-Encoding: *\r\n"
         "Accept-Language: en-us\r\n"
         "TE:\r\n"
         "User-Agent: Boundary Device\r\n"
         "\r\n"
      };
      int const numToSend = strlen( getMsg );
      int const numSent = send( getFd(), getMsg, numToSend, 0 );
      printf( "%d of %d bytes sent\n", numSent, numToSend );
   }
   setMask( getMask() & ~POLLOUT );
}

void urlPoll_t :: onError( void )         // POLLERR
{
   printf( "Error\n" );
   checkStatus();
   state_ = closed ;
   close();
}

void urlPoll_t :: onHUP( void )           // POLLHUP
{
   printf( "HUP\n" );
   checkStatus();
   state_ = closed ;
   close();
}

void urlPoll_t :: checkStatus( void )
{
   int err ;
   socklen_t errSize ;
   if( ( 0 == getsockopt( getFd(), SOL_SOCKET, SO_ERROR, (void *)&err, &errSize) )
       &&
       ( ( 0 == err ) || ( EISCONN == err ) ) )
   {
      if( connecting == state_ )
         state_ = connected ;
   }
   else
   {
      close();
      state_ = closed ;
   }
}

#ifdef __MODULETEST__

#include <arpa/inet.h>

int main( int argc, char const * const argv[] )
{
   if( 1 < argc )
   {
      pollHandlerSet_t handlers ;
      urlPoll_t **urls = new urlPoll_t *[argc-1];

      unsigned numAlloc = 0 ;
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         in_addr server ;
         if( 0 != inet_aton( argv[arg], &server ) )
         {
            printf( "polling IP adress %s:%lx\n", argv[arg], server.s_addr );
            urlPoll_t *url = new urlPoll_t( server.s_addr, htons( 80 ), handlers );
            if( urlPoll_t::closed != url->getState() )
            {
               urls[arg-1] = url ;
               numAlloc++ ;
            }
            else
               delete url ;
         }
         else
            fprintf( stderr, "invalid IP address <%s>\n", argv[arg] );
      }

      int iterations = 0 ;
      while( 0 < numAlloc )
      {
         if( handlers.poll( 1000 ) )
            printf( "poll %d\n", ++iterations );
         else
            printf( "idle:%d\n", ++iterations );
         for( int i = 1 ; i < argc ; i++ )
         {
            urlPoll_t *url = urls[i-1];
            if( url )
            {
               if( urlPoll_t::closed != url->getState() )
               {
                  printf( "%u still alive\n", i );
               }
               else
               {
                  printf( "closing %u\n", i );
                  delete url ;
                  urls[i-1] = 0 ;
                  --numAlloc ;
               }
            }
         }
      }
   }
   else
      fprintf( stderr, "Usage: urlPoll server [server...]\n" );

   return 0 ;
}
#endif
