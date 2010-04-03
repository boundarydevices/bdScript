#include <sys/socket.h>
#include <stdio.h>
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
#include <stdlib.h>

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      int const sFd = socket( AF_INET, SOCK_STREAM, 0 );
      if( 0 <= sFd )
      {
         sockaddr_in myAddress ;
   
         memset( &myAddress, 0, sizeof( myAddress ) );
   
         myAddress.sin_family      = AF_INET;
         myAddress.sin_addr.s_addr = 0 ; // local
         myAddress.sin_port        = htons(strtoul(argv[1],0,0));
   
         int doit = 1 ;
         int result = setsockopt( sFd, SOL_SOCKET, SO_REUSEADDR, &doit, sizeof( doit ) );
         if( 0 != result )
            fprintf( stderr, "SO_REUSEADDR:%d:%m\n", result );
         
         if( 0 == bind( sFd, (struct sockaddr *) &myAddress, sizeof( myAddress ) ) )
         {
            if( 0 == listen( sFd, 20 ) )
            {
               while( 1 )
               {
                  struct sockaddr_in clientAddr ;
                  socklen_t sockAddrSize = sizeof( clientAddr );
                  int newFd = ::accept( sFd, (struct sockaddr *)&clientAddr, &sockAddrSize );
                  if( 0 <= newFd )
                  {
                     char inBuf[2048];
                     int  numRead ;
                     while( 0 < ( numRead = read( newFd, inBuf, sizeof(inBuf ) ) ) )
                     {
                        write( newFd, inBuf, numRead );
                     }
                     close( newFd );
                  }
                  else
                  {
                     perror( "accept" );
                     break ;
                  }
               }
            }
            else
               perror( "listen" );
         }
         else
            perror( "bind" );

         close( sFd );
      }
      else
         perror( "socket" );
   }
   else
      fprintf( stderr, "Usage: tcpEcho portNum\n" );
      
   return 0 ;
}
