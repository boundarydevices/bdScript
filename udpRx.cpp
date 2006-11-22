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
   local.sin_port        = 0xBDBD ;
   bind( fd, (struct sockaddr *)&local, sizeof( local ) );
   int doit = 1 ;
   setsockopt( fd, SOL_SOCKET, SO_BROADCAST, &doit, sizeof( doit ) );

   char inBuf[512];
   int numRead ;
   while( 0 < ( numRead = recv( fd, inBuf, sizeof(inBuf), 0 ) ) )
   {
      printf( "rx %u bytes\n", numRead );
      fwrite( inBuf, numRead, 1, stdout );
      printf( "\n" );
   }

   return 0 ;
}

