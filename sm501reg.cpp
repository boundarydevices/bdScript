/*
 * Program sm501reg.cpp
 *
 * This program reads a register value from the SM-501
 *
 *
 * Change History : 
 *
 * $Log: sm501reg.cpp,v $
 * Revision 1.1  2006-05-07 15:42:14  ericn
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
	printf( "Hello, %s\n", argv[0] );
	if( 1 < argc ){
		unsigned long reg = strtoul(argv[1], 0, 0 );
		printf( "register[0x%08lx] == ", reg );
		int const fbDev = open( "/dev/fb/0", O_RDWR );
		if( 0 <= fbDev ) {
			int res = ioctl( fbDev, SM501_READREG, &reg );
			if( 0 == res ){
				printf( "%08lx", reg );
			}
			else
				perror( "SM501_READREG" );
			close( fbDev );
		}
		else
			perror( "/dev/fb/0" );
		printf( "\n" );
	}
	else
		fprintf( stderr, "Usage: %s 0xregister\n", argv[0] );
	return 0 ;
}
