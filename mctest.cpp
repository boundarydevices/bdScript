/*
 * Program to test new [sg]etsockopts and ioctls for manipulating IP and
 * Ethernet multicast address filters.
 * You can watch the IGMP messages on the network by also running a trace
 * tool such as tcpdump.
 *
 * Written by Steve Deering, Stanford University, February 1989.
 *
 * The code compiles "as-is" under 4.4BSD.
 * For Solaris 2.5 you need to add a #include of <sys/sockio.h> and then
 * "cc mtest.c -lsocket -lnsl".
 */

#define MULTICAST

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "rawKbd.h"
#include <sys/poll.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define HELLO_PORT 2020
static char message[] = {
	"Hello\n"
};

int main( int argc, char **argv )
{
	if( 3 > argc ){
		fprintf( stderr, "Usage: %s multicast_addr local_addr [port=2020]\n", argv[0] );
		return -1 ;
	}
	unsigned long mc_addr = inet_addr(argv[1]);
	unsigned long if_addr = inet_addr(argv[2]);
	printf( "receive multicast from 0x%08lx on interface 0x%08lx\n", mc_addr, if_addr );
	
	int so;
	if( (so = socket( AF_INET, SOCK_DGRAM, 0 )) == -1) {
		perror( "can't open socket" );
		return -1 ;
	}
	struct ip_mreq imr;
	imr.imr_multiaddr.s_addr = mc_addr ;
	imr.imr_interface.s_addr = if_addr ;
	if( 0 != setsockopt( so, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq) ) ){
		perror( "can't join group" );
		return -1 ;
	}

        unsigned short port = (3 < argc) ? strtoul(argv[3],0,0) : HELLO_PORT ;

	/* set up destination address */
	struct sockaddr_in addr;
     	memset(&addr,0,sizeof(addr));
	addr.sin_family=AF_INET;
	addr.sin_addr.s_addr=htonl(INADDR_ANY); /* N.B.: differs from sender */
	addr.sin_port=htons(port);
	
	/* bind to receive address */
	if(bind(so,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
		perror("bind");
		return -1 ;
	}

        fcntl( so, F_SETFL, O_NONBLOCK );

	printf( "joined group and bound to port %u (0x%x)\n", port, port );
        rawKbd_t keyboard ;

	#define NUMFDS 2
	struct pollfd fds[NUMFDS];
	fds[0].fd = 0 ;
	fds[0].events = POLLIN|POLLERR|POLLHUP ;
	fds[1].fd = so ;
	fds[1].events = POLLIN|POLLERR|POLLHUP ;
	while( 1 ){
		int numReady = poll( fds, NUMFDS, -1 );
		if( 0 < numReady ){
			printf( "%d fds ready\n", numReady );
			for( int i = 0 ; i < NUMFDS ; i++ ){
				if( fds[i].revents & POLLIN ){
					printf( "%d is ready for read\n", i );
					if( 0 == fds[i].fd ){
						char in ;
						while( keyboard.read(in) ){
							switch( in ){
								case '\x03': {
									printf( "<ctrl-c>\n" );
									return 0 ;
								}
								case 's': {
									printf( "send stuff here\n" );
									addr.sin_addr.s_addr=mc_addr ;
									int numSent ;
                                                                        if ((numSent = sendto(so,message,sizeof(message)-1,0,(struct sockaddr *) &addr, sizeof(addr))) < 0) {
										perror("sendto");
                                                                                return -1 ;
									}
									printf( "sent %d bytes\n", numSent );

									break;
								}
								default: {
									printf( "options:\n"
										"	<ctrl-c>	- bail out\n" 
										"	s		- send something\n" 
										);
								}
							}
						}
					} else {
						char inBuf[65536];
						int numRead = read(fds[i].fd,inBuf,sizeof(inBuf)-1);
						if( 0 < numRead ){
							inBuf[numRead] = '\0' ;
							printf( "%d bytes\n", numRead );
						} else {
							fprintf( stderr, "read(%d)\n", fds[i].fd );
							return -1 ;
						}
					}
				}
			}
		}
		else {
			fprintf( stderr, "pollerr:%m\n" );
			return -1 ;
		}
	}

	return 0 ;
}
