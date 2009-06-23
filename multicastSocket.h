#ifndef __MULTICASTSOCKET_H__
#define __MULTICASTSOCKET_H__ "$Id$"

/*
 * multicastSocket.h
 *
 * This header file declares the multicastSocket routine, which 
 * is a thin wrapper to peform the boilerplate code for creating
 * a multi-cast UDP socket
 *
 *	- create a UDP socket	
 *			socket(AF_INET,SOCK_DGRAM,0);
 *	- call the IP_ADD_MEMBERSHIP sockopt 
 *			setsockopt( udpSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq) );
 *
 * This routine doesn't bind() so it's useful for apps that just transmit.
 *
 * Change History : 
 *
 * $Log$
 *
 *
 * Copyright Boundary Devices, Inc. 2009
 */

#include <netinet/in.h>

int multicastSocket( unsigned long ipv4_group_addr,
		     unsigned long ipv4_local_addr = INADDR_ANY );

#endif

