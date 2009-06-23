#include <stdio.h>
#include "imgJPEG.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>
#include <sys/mman.h>
#include <time.h>
#include <poll.h>
#include "hexDump.h"
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "tickMs.h"
#include "fbDev.h"
#include <errno.h>
#include <stddef.h>
#include "imgJPEG.h"
#include "jpegToYUV.h"
#include "multicastSocket.h"
#include "pxaYUV.h"

extern "C" {
#include <jpeglib.h>
};

static bool volatile die = false ;

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   die = true ;
}


#define VIDEO_PALETTE_BAYER	(('q'<<8) | 1)	/* Grab video in raw Bayer format */
#define VIDEO_PALETTE_MJPEG	(('q'<<8) | 2)	/* Grab video in compressed MJPEG format */

int main( int argc, char const * const argv[] )
{
   signal( SIGINT, ctrlcHandler );

   if( 3 == argc )
   {
      int const targetIP = inet_addr( argv[1] );
      if( INADDR_NONE != targetIP )
      {
         unsigned short const targetPort = strtoul( argv[2], 0, 0 );
         sockaddr_in remote ;
         remote.sin_family      = AF_INET ;
         remote.sin_addr.s_addr = targetIP ;
         remote.sin_port        = htons(targetPort) ;

         int const udpSock = multicastSocket(targetIP);
         if( 0 <= udpSock )
         {
            printf( "joined multicast group %s, port %u\n", argv[1], targetPort );
            sockaddr_in local ;
            local.sin_family      = AF_INET ;
            local.sin_addr.s_addr = htonl(INADDR_ANY); /* N.B.: differs from sender */
            local.sin_port        = htons(targetPort);
            bind( udpSock, (struct sockaddr *)&local, sizeof( local ) );

            printf( "socket bound\n" );
            #define NUMFDS 1
            struct pollfd fds[NUMFDS];
            fds[0].fd = udpSock ;
            fds[0].events = POLLIN ;

            char inBuf[65536];

            fbDevice_t &fb = getFB();
            pxaYUV_t *yuv = 0 ;

            unsigned long packets = 0 ;
            unsigned long skipped = 0 ;

            while( !die ){
               int numReady = poll(fds,NUMFDS,-1);
               if( 0 < numReady ){
                  if( fds[0].revents & POLLIN ){
                     sockaddr_in fromAddr ;
                     socklen_t   fromSize = sizeof( fromAddr );
                     int numRead = recvfrom( udpSock, inBuf, sizeof( inBuf ), 0, (struct sockaddr *)&fromAddr, &fromSize );
                     if( 0 < numRead ){
                        ++packets ;
                        int numAvail = -1 ;
                        int rval = ioctl(udpSock,FIONREAD,&numAvail);
                        if(rval){
                           perror("FIONREAD");
                           numAvail = -1 ;
                        }
                        printf( "rx: %d: %d\n", numRead, numAvail );
                        if( 0 < numAvail ){
                           ++skipped ;
                           continue;
                        }
                        unsigned short width ;
                        unsigned short height ;
                        void const *pixData ;
//                        if( imageJPEG( inBuf, numRead, pixData, width, height ) ){
                        if( jpegToYUV( inBuf, numRead, pixData, width, height ) ){
                           printf( "    decoded: %ux%u\n", width, height );
//                           fb.render( 0, 0, width, height, (unsigned short const *)pixData );
                           if( yuv && ((width != yuv->getWidth()) ||(height != yuv->getHeight())) ){
                              delete yuv ;
                              yuv = 0 ;
                           }

                           if( 0 == yuv )
                              yuv = new pxaYUV_t(0,0,width,height);

                           if(yuv && yuv->isOpen())
                              yuv->writeInterleaved((unsigned char *)pixData);

                           delete [] (unsigned short *)pixData ;
                        }
                        else
                           printf( "    decode error\n" );
                     }
                  }
               }
               else
                  fprintf( stderr, "poll:%m\n" );
            }
            unsigned long numRead = packets - skipped ;
            printf( "read %lu of %lu packets (%u percent)\n", numRead, packets, (numRead*100)/packets );
         }
         else
            printf( "socketerr: %m\n" );
      }
      else
         printf( "Invalid target IP address %s\n", argv[1] );
   }
   else
      printf( "Usage: %s targetip(dotted decimal) port\n", argv[0] );

   return 0 ;
}
