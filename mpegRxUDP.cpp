/*
 * Module mpegRxUDP.cpp
 *
 * This module defines the methods of the mpegRxUDP_t 
 * class as declared in mpegRxUDP.h
 *
 *
 * Change History : 
 *
 * $Log: mpegRxUDP.cpp,v $
 * Revision 1.2  2006-08-27 18:14:24  ericn
 * -remove unsupported dts
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "mpegRxUDP.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

mpegRxUDP_t::mpegRxUDP_t( unsigned short portNum )
   : sFd_( socket( AF_INET, SOCK_DGRAM, 0 ) )
   , prevId_(UINT_MAX)
   , prevSeq_(UINT_MAX)
   , totalBytes_( 0 )
   , videoBytes_( 0 )
   , audioBytes_( 0 )
{
   fileName_[0] = '\0' ;
   if( 0 <= sFd_ ){
      sockaddr_in local ;
      local.sin_family      = AF_INET ;
      local.sin_addr.s_addr = INADDR_ANY ;
      local.sin_port        = htons(portNum) ;
   
      if( 0 == bind( sFd_, (struct sockaddr *)&local, sizeof( local ) ) ){
         fcntl( sFd_, F_SETFD, FD_CLOEXEC );
         fcntl( sFd_, F_SETFL, O_NONBLOCK );
         return ;
      }
      else
         perror( "mpegRxUDP bind" );
   
      close( sFd_ );
      sFd_ = -1 ;
   }
   else
      perror( "mpegRxUDP socket" );
}

mpegRxUDP_t::~mpegRxUDP_t( void )
{
   if( isBound() ){
      close( sFd_ );
      sFd_ = -1 ;
   }
}

void mpegRxUDP_t::onNewFile( 
   char const *fileName,
   unsigned    fileNameLen )
{
   printf( "file " ); fwrite( fileName, fileNameLen, 1, stdout ); printf( "\n" );
}

void mpegRxUDP_t::onRx( 
   bool                 isVideo,
   bool                 discont,
   unsigned char const *fData,
   unsigned             length,
   long long            pts )
{
}

void mpegRxUDP_t::onEOF( 
   char const   *fileName,
   unsigned long totalBytes,
   unsigned long videoBytes,
   unsigned long audioBytes )
{
   printf( "eof: %s, %lu bytes (%lu video, %lu audio)\n", fileName, totalBytes, videoBytes, audioBytes );
}

void mpegRxUDP_t::onEOF( void )
{
   onEOF( fileName_, totalBytes_, videoBytes_, audioBytes_ );
   totalBytes_ = videoBytes_ = audioBytes_ = 0 ;
}

void mpegRxUDP_t::poll( void )
{
   mpegUDP_t   inPacket ;
   sockaddr_in fromAddr ;
   socklen_t   fromSize = sizeof( fromAddr );
   do {
      int numRead = recvfrom( sFd_,
                              (char *)&inPacket, sizeof(inPacket), 
                              MSG_DONTWAIT,
                              (struct sockaddr *)&fromAddr, 
                              &fromSize );
      if( 0 < numRead ){
//         printf( "rx: %u.%u ", inPacket.identifier_, inPacket.sequence_ );
         switch( (unsigned)inPacket.type_ ){
            case MPEGUDP_HEADER:
            {
               mpegHeader_t &header = inPacket.d.header_ ;

               if( ( 0 != totalBytes_ ) || ( 0 != fileName_[0] ) ){
                  onEOF();
                  prevId_ = inPacket.identifier_ ;
               }

               memcpy( fileName_, header.name_, header.nameLength_ );
               fileName_[header.nameLength_] = '\0' ;

               onNewFile( header.name_, header.nameLength_ );

               prevId_  = inPacket.identifier_ ;
               prevSeq_ = inPacket.sequence_ ;
               break ;
            }

            case MPEGUDP_DATA:
            {
               if( prevId_ != inPacket.identifier_ ){
                  onEOF();
                  prevSeq_ = 0 ;
                  prevId_ = inPacket.identifier_ ;
               }

               int delta = inPacket.sequence_ - prevSeq_ ;
               if( 0 < delta ){
                  bool const discontinuity = (1 != delta);

                  mpegFrame_t &frame = inPacket.d.frame_ ;
                  totalBytes_ += frame.frameLen_ ;
                  if( MPEGUDP_AUDIO == frame.type_ )
                     audioBytes_ += frame.frameLen_ ;
                  else if( MPEGUDP_VIDEO == frame.type_ )
                     videoBytes_ += frame.frameLen_ ;
   
                  onRx( ( MPEGUDP_VIDEO == frame.type_ ), 
                        discontinuity, 
                        frame.data_, frame.frameLen_,
                        frame.pts_ );
                  if( discontinuity )
                     printf( "discontinuity: %u/%u\n", prevSeq_, inPacket.sequence_ );
                  prevSeq_ = inPacket.sequence_ ;
               } // not an obsolete packet
               else
                  printf( "obsolete packet\n" );

/*
               printf( "data type %d, stream %u, length %u, %llu/%llu\n", 
                       frame.type_, frame.streamId_,
                       frame.frameLen_,
                       frame.dts_, frame.pts_ );
*/
               break ;
            }
            default:{
               printf( "invalid type %u\n", inPacket.type_ );
            }
         }
      }
      else {
         if( EAGAIN != errno ) {
            fprintf( stderr, "recv:%d:%m\n", errno );
         }
         break ;
      }
   } while( 1 );;
}


#ifdef MODULETEST

#include "rtSignal.h"
#include <signal.h>
#include <openssl/md5.h>

class mpegRxUDP_MD5_t : public mpegRxUDP_t {
public:
   mpegRxUDP_MD5_t( unsigned short port );
   ~mpegRxUDP_MD5_t( void );

   virtual void onNewFile( char const *fileName,
                           unsigned    fileNameLen );
   virtual void onRx( bool                 isVideo,
                      bool                 discont,
                      unsigned char const *fData,
                      unsigned             length,
                      long long            pts );
   virtual void onEOF( char const   *fileName,
                       unsigned long totalBytes,
                       unsigned long videoBytes,
                       unsigned long audioBytes );

   unsigned audioPackets ;
   unsigned videoPackets ;
   unsigned otherPackets ;
   MD5_CTX  videoMD5 ;
};

mpegRxUDP_MD5_t::mpegRxUDP_MD5_t( unsigned short port )
   : mpegRxUDP_t( port )
   , videoPackets( 0 )
{
   videoMD5.num=-1;
}

mpegRxUDP_MD5_t::~mpegRxUDP_MD5_t( void )
{
}

void mpegRxUDP_MD5_t::onNewFile
   ( char const *fileName,
     unsigned    fileNameLen )
{
   mpegRxUDP_t::onNewFile(fileName, fileNameLen);
   MD5_Init( &videoMD5 );
   videoPackets = 0 ;
   audioPackets = 0 ;
   otherPackets = 0 ;
}

void mpegRxUDP_MD5_t::onRx
   ( bool                 isVideo,
     bool                 discont,
     unsigned char const *fData,
     unsigned             length,
     long long            pts )
{
   mpegRxUDP_t::onRx( isVideo, discont, fData, length, pts );
   if( isVideo ){
      videoPackets++ ;
      if( 0 <= videoMD5.num ){
         MD5_Update( &videoMD5, fData, length );
      }
   }
   else
      audioPackets++ ;
}

void mpegRxUDP_MD5_t::onEOF( 
   char const   *fileName,
   unsigned long totalBytes,
   unsigned long videoBytes,
   unsigned long audioBytes )
{
   mpegRxUDP_t::onEOF( fileName, totalBytes, videoBytes, audioBytes );
   if( 0 <= videoMD5.num ){
      unsigned char md5[MD5_DIGEST_LENGTH];
      printf( "   %u bytes, %u packets (%u audio)\n", videoMD5.num, videoPackets, audioPackets );

      MD5_Final( md5, &videoMD5 );
      printf( "   md5: " );
      for( unsigned i = 0 ; i < MD5_DIGEST_LENGTH ; i++ )
         printf( "%02x ", md5[i] );
      printf( "\n" );
      videoMD5.num=-1;
      videoPackets = 0 ;
   }
}

static void rxHandler( int signo, siginfo_t *info, void *context )
{
}

int main( int argc, char const * const argv[] )
{
   if( 2 == argc ){
      unsigned long portNum = strtoul( argv[1], 0, 0 );
      if( portNum && ( USHRT_MAX > portNum ) ){
         mpegRxUDP_MD5_t rxUDP( (unsigned short)portNum );
         if( rxUDP.isBound() ){
            int const rxSignal = nextRtSignal();
         
            sigset_t signals ;
            sigemptyset( &signals );
            sigaddset( &signals, rxSignal );
         
            struct sigaction sa ;
            sa.sa_flags = SA_SIGINFO|SA_RESTART ;
            sa.sa_restorer = 0 ;
            sigemptyset( &sa.sa_mask );
         
            fcntl(rxUDP.getFd(), F_SETOWN, getpid());
            fcntl(rxUDP.getFd(), F_SETSIG, rxSignal );
            sa.sa_sigaction = rxHandler ;
            sigaction(rxSignal, &sa, 0 );
            
            int flags = fcntl( rxUDP.getFd(), F_GETFL, 0 );
            fcntl( rxUDP.getFd(), F_SETFL, flags | O_NONBLOCK | FASYNC );

            while( 1 ){
               rxUDP.poll();
               sleep( 1 );
            }
         }
         else
            fprintf( stderr, "bind: %m" );
      }
      else
         fprintf( stderr, "Invalid port number %s\n", argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s portNumber\n", argv[0] );

   return 0 ;
}

#endif
