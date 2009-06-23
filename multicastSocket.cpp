/*
 * Module multicastSocket.cpp
 *
 * This module defines the multicastSocket() routine
 * as declared in multicastSocket.h
 *
 * Change History : 
 *
 * $Log$
 *
 * Copyright Boundary Devices, Inc. 2009
 */

#include "multicastSocket.h"
#include <stdio.h>
#include <unistd.h>

int multicastSocket( unsigned long mcast_addr, unsigned long local_addr )
{
	int sockFd = socket(AF_INET,SOCK_DGRAM,0 );
	if( 0 <= sockFd ){
	      struct ip_mreq imr;
	      imr.imr_multiaddr.s_addr = mcast_addr ;
	      imr.imr_interface.s_addr = local_addr ;
	      if( 0 != setsockopt( sockFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof(struct ip_mreq) ) ){
		 perror( "can't join group" );
                 close(sockFd);
		 sockFd = -1 ;
	      }
	}

	return sockFd ;
}
