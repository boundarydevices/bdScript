#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static unsigned const RXSIZE = 16384 ;

int main( int argc, char const * const argv[] )
{
   if( 3 == argc )
   {
      struct in_addr localIP ;
      if( inet_aton( argv[1], &localIP ) )
      {
         char *end ;
         unsigned long localPort = strtoul( argv[2], &end, 0 );
         if( ( 0 < localPort ) && ( 65536 > localPort )
             &&
             ( '\0' == *end ) )
         {
            printf( "IP %s, port 0x%04x\n", inet_ntoa( localIP ), localPort );
            int const sFd = socket( AF_INET, SOCK_DGRAM, 0 );
            if( 0 <= sFd )
            {
               sockaddr_in local ;
               local.sin_family = AF_INET ;
               local.sin_addr.s_addr = INADDR_ANY ;
               local.sin_port   = localPort ;
               if( 0 == bind( sFd, (struct sockaddr *) &local, sizeof( local ) ) )
               {
                  char * const rxBuf = new char [ RXSIZE ];
                  while( 1 )
                  {
                     sockaddr_in remote ;
                     socklen_t   remSize = sizeof( remote );
                     int const numRx = recvfrom( sFd, rxBuf, RXSIZE, 0, 
                                                 (struct sockaddr *)&remote, &remSize );
                     if( 0 < numRx  )
                     {
                        printf( "%u\n", numRx );
//                        printf( "%c", rxBuf[0] ); fflush( stdout );
                     }
                     else
                     {
                        fprintf( stderr, "rxErr %m" );
                        break;
                     }
                  }
               }
               else
                  fprintf( stderr, "bind:%m\n" );
               close( sFd );
            }
            else
               fprintf( stderr, "socketErr:%m\n" );
         }
         else
            fprintf( stderr, "Invalid port <%s>\n", argv[2] );
      }
      else
         fprintf( stderr, "Invalid ip <%s>\n", argv[1] );

   }
   else
      fprintf( stderr, "usage: udpSpewTo ip(dotted decimal) port\n" );

   return 0 ;
}
