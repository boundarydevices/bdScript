#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

static unsigned const SENDSIZE = 16384 ;

int main( int argc, char const * const argv[] )
{
   if( 3 == argc )
   {
      struct in_addr targetIP ;
      if( inet_aton( argv[1], &targetIP ) )
      {
         char *end ;
         unsigned long targetPort = strtoul( argv[2], &end, 0 );
         if( ( 0 < targetPort ) && ( 65536 > targetPort )
             &&
             ( '\0' == *end ) )
         {
            printf( "IP %s, port 0x%04x\n", inet_ntoa( targetIP ), targetPort );
            int const sFd = socket( AF_INET, SOCK_DGRAM, 0 );
            if( 0 <= sFd )
            {
               sockaddr_in remote ;
               remote.sin_family = AF_INET ;
               remote.sin_addr   = targetIP ;
               remote.sin_port   = htons(targetPort) ;

               unsigned char * const sendBuf = new unsigned char [ SENDSIZE ];
               char cNext = '0' ;
               while( 1 )
               {
                  memset( sendBuf, cNext, SENDSIZE );
                  int const numSent = sendto( sFd, sendBuf, SENDSIZE, 0, 
                                              (struct sockaddr *)&remote, sizeof( remote ) );
                  if( numSent == SENDSIZE )
                  {
                     printf( "t" ); fflush( stdout );
                     cNext++ ;
                     if( '9' < cNext )
                        cNext = '0' ;

                     struct timespec tv ;
                     tv.tv_sec  = 0 ;
                     tv.tv_nsec = 100000000 ;
               
               //      JS_ReportError( cx, "nanosleep %f : %u.%lu seconds\n", seconds, tv.tv_sec, tv.tv_nsec );
               
                     nanosleep( &tv, 0 );

                  }
                  else
                  {
                     fprintf( stderr, "sendToErr %m" );
                     break;
                  }
               }
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
