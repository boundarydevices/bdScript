/*
 * Program sm501flip.cpp
 *
 * This program reads a register value from the SM-501
 *
 *
 * Change History : 
 *
 * $Log: sm501flip.cpp,v $
 * Revision 1.1  2006-05-07 15:41:59  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#define irqreturn_t int
#include <linux/sm501-int.h>

int main( int argc, char const * const argv[] )
{
	int const fbDev = open( "/dev/fb/0", O_RDWR );
	if( 0 <= fbDev ) {
		unsigned long whichFB = 0xFFFFFFFF ;
		int res = ioctl( fbDev, SM501_FLIPBUFS, &whichFB );
		if( 0 == res ){
			printf( "flipped: fb is %ld\n", whichFB );
		}
		else
			perror( "SM501_FLIPBUFS" );
		close( fbDev );
	}
	else
		perror( "/dev/fb/0" );
	return 0 ;
}
