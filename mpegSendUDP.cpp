/*
 * Program mpegSendUDP.cpp
 *
 * This program will parse and send MPEG data over 
 * (possibly broadcast) UDP.
 *
 * Change History : 
 *
 * $Log: mpegSendUDP.cpp,v $
 * Revision 1.4  2006-08-27 19:13:49  ericn
 * -use mpegFileStream_t
 *
 * Revision 1.3  2006/08/26 16:07:00  ericn
 * -show PTS range for each file
 *
 * Revision 1.2  2006/08/20 19:40:52  ericn
 * -keep track of biggest packet
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

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
#include "mpegStream.h"
#include "mpegUDP.h"
#include "tickMs.h"

int main( int argc, char const * const argv[] )
{
   if( 4 <= argc )
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

               int sockopt = 1;
               setsockopt( sFd, SOL_SOCKET, SO_BROADCAST, (char *) &sockopt, sizeof(sockopt));

               unsigned nextFileId = 0 ;
               unsigned arg = 3 ; 
               while( 1 ){
                  char const *fileName = argv[arg];
                  FILE *const fIn = fopen( fileName, "rb" );
                  if( fIn )
                  {
                     mpegUDP_t outPacket ;
                     memset( &outPacket, 0, sizeof(outPacket) );

                     outPacket.type_ = MPEGUDP_HEADER ;
                     outPacket.identifier_ = nextFileId++ ;
                     outPacket.sequence_   = 0 ;

                     mpegHeader_t &header = outPacket.d.header_ ;
                     char *name = strrchr( fileName, '/' );
                     if( 0 != name )
                        name++ ;
                     else
                        name = (char *)fileName ;

                     header.nameLength_ = strlen(name);
                     if( header.nameLength_ >= sizeof( header.name_ ) )
                        header.nameLength_ = sizeof( header.name_ )-1 ;
                     memcpy( header.name_, name, header.nameLength_ );
                     header.name_[header.nameLength_] = '\0' ;

                     unsigned headerLength = (header.name_ + header.nameLength_ + 1 )
                                             - (char *)&outPacket ;

                     long long startTick = tickMs();
                     int numSent = sendto( sFd, &outPacket, headerLength, 0, 
                                           (struct sockaddr *)&remote, sizeof( remote ) );
                     if( numSent != headerLength )
                        perror( "send header" );

                     printf( "--- sending " ); fwrite( header.name_, header.nameLength_, 1, stdout ); printf( "\n" );

                     long long firstPTS = 0LL ;
                     long long lastPTS = 0LL ;
                     mpegFrame_t &frame = outPacket.d.frame_ ;
                     outPacket.type_ = MPEGUDP_DATA ;
                     outPacket.sequence_++ ;
                     
                     mpegStreamFile_t stream( fIn );
                     unsigned long        videoBytes = 0 ;
                     unsigned long        videoPackets = 0 ;

                     long long firstTS = 0 ;
                     unsigned maxData = 0 ;
                     unsigned char const *frameData ;
                     unsigned             frameLen ;
                     CodecType            type ;
                     CodecID              codecId ;
                     unsigned char        streamId ;

                     while( stream.getFrame( frameData, frameLen, frame.pts_, streamId, type, codecId ) ){
//                        printf( "%d/%p/%u/%llu\n", type, frameData, frameLen, frame.pts_ );
                        frame.pts_ /= 90 ;
                        if( 0LL == firstPTS )
                           firstPTS = frame.pts_ ;
                        if( 0 != frame.pts_ )
                           lastPTS = frame.pts_ ;
                        if( 0 != frame.pts_ ){
                           if( 0 == firstTS ){
                              firstTS = tickMs() - frame.pts_ ;
                           }
                           else {
                              long long when = frame.pts_+firstTS ;
                              long long now = tickMs();
                              while( 0 > (int)(now - when) ){
                                 long int delay = (long int)(when-now);
                                 usleep( delay*1000 );
                                 now = tickMs();
                              }
                           }
                        } // this packet has a timestamp

                        frame.type_ = (mpegFrameType_e)(type+1);
                        frame.frameLen_ = frameLen ;
                        frame.streamId_ = streamId ;
                        if( frameLen < MPEGUDP_MAXDATA ){
                           if( maxData < frameLen )
                              maxData = frameLen ;
                           memcpy( frame.data_, frameData, frameLen );
                           int msgSize = (frame.data_+frameLen) - (unsigned char *)&outPacket ;
                           numSent = sendto( sFd, &outPacket, msgSize, 0, 
                                             (struct sockaddr *)&remote, sizeof( remote ) );
                           if( numSent != msgSize )
                              fprintf( stderr, "short write: %u/%u\n", numSent, msgSize );
                           if( MPEGUDP_VIDEO == frame.type_ ){
                              videoBytes += frameLen ;
                              videoPackets++ ;
                           }
                        }
                        else
                           fprintf( stderr, "frame too big: %u/%u\n",
                                    frameLen, MPEGUDP_MAXDATA );

                        ++outPacket.sequence_ ;
                     }

                     long long endTick = tickMs();
                     printf( "sent %s in %ld ms\n"
                             "    (%lu bytes, %lu packets of video)\n"
                             "    biggest: %u bytes\n"
                             "    pts range: %u (%llu->%llu)\n", 
                             fileName, (long)(endTick-startTick), 
                             videoBytes, videoPackets, 
                             maxData,
                             (unsigned)(lastPTS-firstPTS),
                             firstPTS, lastPTS
                              );
                     sleep(1);
                     fclose( fIn );
                  }
                  else
                     perror( fileName );
                  
                  if( ++arg >= argc )
                     arg = 3 ; // repeat
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
      fprintf( stderr, "usage: mpegSendUDP ip(dotted decimal) port msg\n" );

   return 0 ;
}

