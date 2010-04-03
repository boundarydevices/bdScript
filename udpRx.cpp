#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main( void )
{
   int const fd = socket( AF_INET, SOCK_DGRAM, 0 );
   sockaddr_in local ;
   local.sin_family      = AF_INET ;
   local.sin_addr.s_addr = INADDR_ANY ;
   local.sin_port        = 0xBDBD;
   bind( fd, (struct sockaddr *)&local, sizeof( local ) );
   int doit = 1 ;
   int result = setsockopt( fd, SOL_SOCKET, SO_BROADCAST, &doit, sizeof( doit ) );
   if( 0 != result )
	   perror( "SO_BROADCAST" );
   doit = 1 ;
   result = setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &doit, sizeof( doit ) );
   if( 0 != result )
      fprintf( stderr, "SO_REUSEADDR:%d:%m\n", result );

   char inBuf[512];
   int numRead ;
   sockaddr_in rem ;
   socklen_t remSize = sizeof(rem);
   while( 0 < ( numRead = recvfrom( fd, inBuf, sizeof(inBuf), 0, (struct sockaddr *)&rem, &remSize ) ) )
   {
      printf( "rx %u bytes from %s, port 0x%x\n", numRead, inet_ntoa(rem.sin_addr), ntohs(rem.sin_port) );
      fwrite( inBuf, numRead, 1, stdout );
      printf( "\n" );

   }

   return 0 ;
}

