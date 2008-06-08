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
#include <sys/poll.h>
#include <arpa/inet.h>
#include <stdlib.h>

static char const goPinPad[] = {
        "6_Please enter your 4-digit PIN number|\n"
};

static char const pinPadResponse[] = {
   "2_|\n"
};

static char const goIdle[] = {
   "1_|\n"
};

static bool isIdle = false ;

static void processInput( int sockFd, char const *inData, unsigned len )
{
   if( ( 0 < len ) && ('\n' == inData[len-1]) ){
      if( '~' == *inData ){
         switch( inData[1] ){
            case 'S': {
               write( sockFd, goPinPad, sizeof(goPinPad)-1 );
               break;
            }
            case 'P': {
               write( sockFd, pinPadResponse, sizeof(pinPadResponse)-1 );
               break;
            }
            case 'C': {
               sleep(1);
               write( sockFd, goPinPad, sizeof(goPinPad)-1 );
               break;
            }
            default: {
               fprintf( stderr, "Unknown msgtype %c\n", inData[1] );
            }
         }
      }
      else
         fprintf( stderr, "No tilde~\n" );
   }
   else
      fprintf( stderr, "Bad length %u or no newline\n", len );
}

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      int const sFd = socket( AF_INET, SOCK_STREAM, 0 );
      if( 0 <= sFd )
      {
         int doit = 1 ;
         int result = setsockopt( sFd, SOL_SOCKET, SO_REUSEADDR, &doit, sizeof( doit ) );
         if( 0 != result )
            fprintf( stderr, "SO_REUSEADDR:%d:%m\n", result );
         
    sockaddr_in myAddress ;
   
         memset( &myAddress, 0, sizeof( myAddress ) );
   
         myAddress.sin_family      = AF_INET;
         myAddress.sin_addr.s_addr = 0 ; // local
         myAddress.sin_port        = htons(strtoul(argv[1],0,0));
   
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
                     int flags = flags = fcntl( newFd, F_GETFL, 0 );
                     fcntl( newFd, F_SETFL, flags | O_NONBLOCK );
                     struct pollfd fds[1]; 
                     fds[0].fd = newFd ;
                     fds[0].events = POLLIN | POLLERR ;
                     while( 1 ){
                        int const numReady = ::poll( fds, 1, 1000 );
                        if( 0 < numReady ){
                           if( fds[0].revents & POLLIN ){
                              char inBuf[2048];
                              int  numRead ;
                              while( 0 < ( numRead = read( newFd, inBuf, sizeof(inBuf ) ) ) )
                              {
                                 fwrite( inBuf, numRead, 1, stdout );
                                 inBuf[numRead] = 0 ;
                                 processInput( newFd, inBuf, numRead );
                              }
                           }
                           else if( fds[0].revents & POLLERR ){
                              printf( "poll(Error)\n" );
                              break;
                           }
                           fds[0].events = POLLIN | POLLERR ;
                        }
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
