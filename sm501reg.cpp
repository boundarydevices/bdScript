/*
 * Program sm501reg.cpp
 *
 * This program reads a register value from the SM-501
 *
 *
 * Change History : 
 *
 * $Log: sm501reg.cpp,v $
 * Revision 1.4  2007-08-08 17:12:01  ericn
 * -[sm501] /dev/fb0 not /dev/fb/0
 *
 * Revision 1.3  2006/08/16 02:27:49  ericn
 * -fix register/value labels
 *
 * Revision 1.2  2006/06/10 16:28:19  ericn
 * -allow setting of registers
 *
 * Revision 1.1  2006/05/07 15:42:14  ericn
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
	if( 1 < argc ){
		unsigned long reg = strtoul(argv[1], 0, 0 );
		printf( "register[0x%08lx] == ", reg );
		int const fbDev = open( "/dev/fb0", O_RDWR );
		if( 0 <= fbDev ) {
			int res = ioctl( fbDev, SM501_READREG, &reg );
			if( 0 == res ){
				printf( "%08lx\n", reg );
                                if( 2 < argc )
                                {
                                    reg_and_value rv ;
                                    rv.reg_ = strtoul(argv[1], 0, 0 );
                                    rv.value_ = strtoul(argv[2], 0, 0 );
                                    printf( "writing value 0x%lx to register 0x%lx\n", rv.value_, rv.reg_ );
                                    res = ioctl( fbDev, SM501_WRITEREG, &rv );
                                    if( 0 == res )
                                       printf( "wrote value 0x%lx to register 0x%lx\n", rv.value_, rv.reg_ );
                                    else
                                       perror( "SM501_WRITEREG" );
                                }
			}
			else
				perror( "SM501_READREG" );
			close( fbDev );
		}
		else
			perror( "/dev/fb0" );
		printf( "\n" );
	}
	else
		fprintf( stderr, "Usage: %s 0xregister\n", argv[0] );
	return 0 ;
}
