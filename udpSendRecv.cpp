#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "memFile.h"
#include <sys/poll.h>

static unsigned const SENDSIZE = 16384 ;

int main( int argc, char const * const argv[] )
{
   if( 4 == argc )
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

               if( '@' != argv[3][0] )
               {
                  int const len = strlen( argv[3] );
                  int const numSent = sendto( sFd, argv[3], len, 0, 
                                              (struct sockaddr *)&remote, sizeof( remote ) );
                  if( numSent == len )
                     printf( "sent %d bytes\n", numSent );
                  else
                     perror( "sendto" );
               }
               else
               {
                  printf( "send file here\n" );
                  char const *fileName = argv[3]+1 ;
                  memFile_t fIn( fileName );
                  if( fIn.worked() )
                  {
                     int const numSent = sendto( sFd, fIn.getData(), fIn.getLength(), 0, 
                                                 (struct sockaddr *)&remote, sizeof( remote ) );
                     if( numSent == fIn.getLength() ){
                        printf( "sent %d bytes\n", numSent );
                     }
                     else
                        perror( "sendto" );
                  }
                  else
                     perror( fileName );
               }

               pollfd  fds_[2];
               fds_[0].fd = sFd ;
               fds_[0].revents = POLLIN | POLLERR | POLLHUP ;
               int numReady = ::poll( fds_, 1, 1000 );
               if( 0 <= numReady ){
                  char inBuf[512];
                  sockaddr_in fromAddr ;
                  socklen_t   fromSize = sizeof( fromAddr );
                  int numRead = recvfrom( sFd, inBuf, sizeof(inBuf)-1, MSG_DONTWAIT,
                                         (struct sockaddr *)&fromAddr, 
                                         &fromSize );
                  if( 0 < numRead ){
                     inBuf[numRead] = 0 ;
                     printf( "read %u bytes <%s>\n", numRead, inBuf );
                  }
                  else
                     perror( "read()" );
               }
               else
                  perror( "poll()" );
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
      fprintf( stderr, "usage: udpSendTo ip(dotted decimal) port msg\n" );

   return 0 ;
}
