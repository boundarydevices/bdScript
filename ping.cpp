/*
 * Module ping.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: ping.cpp,v $
 * Revision 1.1  2003-08-23 02:50:26  ericn
 * -added Javascript ping support
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "ping.h"
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/signal.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>

static const int DEFDATALEN = 56;
static const int MAXIPLEN = 60;
static const int MAXICMPLEN = 76;
static const int MAXPACKET = 65468;
#define	MAX_DUP_CHK	(8 * 128)
static const int MAXWAIT = 10;
static const int PINGINTERVAL = 1;		/* second */

#define O_QUIET         (1 << 0)

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

static int in_cksum(unsigned short *buf, int sz)
{
	int nleft = sz;
	int sum = 0;
	unsigned short *w = buf;
	unsigned short ans = 0;

	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(unsigned char *) (&ans) = *(unsigned char *) w;
		sum += ans;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	ans = ~sum;
	return (ans);
}

void *pinger_thread( void *arg )
{
   pinger_t &pinger = *( pinger_t * )arg ;
   struct pollfd fds ;
   fds.fd = pinger.fd_ ;
   fds.events = POLLIN|POLLERR|POLLHUP ;

   bool timedOut = true ;

   do {
      if( timedOut )
      {
	struct sockaddr_in pingaddr;
	memset(&pingaddr, 0, sizeof(pingaddr));
	pingaddr.sin_family = AF_INET;
	memcpy(&pingaddr.sin_addr, &pinger.targetAddr_, sizeof(pingaddr.sin_addr));
	
	char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];
        struct icmp *pkt = (struct icmp *) packet;
	memset(pkt, 0, sizeof(packet));
	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_cksum = in_cksum((unsigned short *) pkt, sizeof(packet));

	sendto(pinger.fd_, packet, sizeof(packet), 0, (struct sockaddr *) &pingaddr, sizeof(struct sockaddr_in));
        timedOut = false ;
      }

      int const pollRes = poll( &fds, 1, 2000 );
      if( 0 < pollRes )
      {
         if( 0 != ( fds.revents & POLLIN ) )
         {
            char inBuf[256];
            struct sockaddr_in from;
            socklen_t fromlen = sizeof(from);

            int const numRx = recvfrom( pinger.fd_, inBuf, sizeof(inBuf), 0, (struct sockaddr *) &from, &fromlen );
            if( 0 <= numRx )
            {
               if( numRx >= 76) /* ip + icmp */
               {
                  struct iphdr const *iphdr = (struct iphdr *)inBuf;
                  struct icmp const *pkt = (struct icmp *)( inBuf + (iphdr->ihl << 2));	/* skip ip hdr */
                  if (pkt->icmp_type == ICMP_ECHOREPLY)
                  {
                     pinger.response( iphdr->saddr );
                  }
                  else if( ICMP_ECHO != pkt->icmp_type ) // not echo of our request
                     printf( " rxother %d\n", pkt->icmp_type );
               }
            }
         }
      }
      else
         timedOut = true ;

   } while( !pinger.cancel_ );

   return 0 ;
}

pinger_t :: pinger_t( char const *hostName )
   : hostName_( strdup( hostName ) ),
     targetAddr_( 0 ),
     errorMsg_( 0 ),
     fd_( -1 ),
     cancel_( false ),
     thread_( (pthread_t)0 )
{
   struct hostent host ;
   char           buf[256];
   int            herrno ;
   struct hostent *h ;
   int result = gethostbyname_r( hostName_,
                        &host, 
                        buf, sizeof( buf), 
                        &h, 
                        &herrno );
   if( 0 == result )
   {
      struct protoent *proto = getprotobyname("icmp");
      fd_ = socket( AF_INET, SOCK_RAW, ( proto ? proto->p_proto : 1));
      if( 0 <= fd_ )
      {
         int sockopt = 1;
         setsockopt( fd_, SOL_SOCKET, SO_BROADCAST, (char *) &sockopt, sizeof(sockopt));

         memcpy( &targetAddr_, h->h_addr, sizeof( targetAddr_ ) );
         if( 0 == pthread_create( &thread_, 0, pinger_thread, this ) )
         {
            return ;
         }
         else
         {
            errorMsg_ = strdup( strerror( errno ) );
            thread_ = (pthread_t)0 ; // just in case
         }
         close( fd_ );
         fd_ = -1 ;
      }
      else
         errorMsg_ = strdup( strerror( errno ) );
   }
   else
      errorMsg_ = strdup( hstrerror( herrno ) );

   cancel_ = true ;

}

pinger_t :: ~pinger_t( void ) // thread is shut down in destructor
{
   
   if( 0 <= fd_ )
   {
      cancel_ = true ;
      int const tempFD = fd_ ;
      fd_ = -1 ;
      close( tempFD );
      if( (pthread_t)0 != thread_ )
      {
         pthread_cancel( thread_ );
         void *exitStat ;
         pthread_join( thread_, &exitStat );
         thread_ = (pthread_t)0 ;
      }
   }
   
   if( hostName_ )
      free( (char *)hostName_ );
   if( errorMsg_ )
      free( (char *)errorMsg_ );
}

void pinger_t :: response( unsigned long ipAddr )
{
}

#ifdef MODULETEST

class myPinger_t : public pinger_t {
public:
   myPinger_t( char const *host )
      : pinger_t( host ){}
   virtual void response( unsigned long ipAddr )
   {
      struct in_addr inaddr ;
      inaddr.s_addr = ipAddr ;

      printf( "response from %s\n", inet_ntoa( inaddr ) );
   }
};

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      myPinger_t pinger( argv[1] );
      if( pinger.initialized() )
      {
         printf( "Hit <enter> to shut down\n" );
         char inBuf[80];
         fgets( inBuf, sizeof( inBuf ), stdin );
      
         return 0 ;
      }
      else
         fprintf( stderr, "Error %s initializing pinger\n", pinger.errorMsg() );
   }
   else
      fprintf( stderr, "Usage: ping ipAddr|hostName\n" );
}
#endif
