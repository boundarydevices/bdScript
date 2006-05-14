#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

typedef int irqreturn_t;

#include <linux/sm501-int.h>

int main( int argc, char const * const argv[] )
{
	int fbDev = open( "/dev/fb/0", O_RDWR );
	if( 0 <= fbDev ) {
		unsigned long startCount = 0xCCCCCCCC ;
		int result = ioctl( fbDev, SM501_GET_SYNCCOUNT, &startCount );
		if( 0 != result )
			printf( "Error reading start count: %m\n" );

		sleep(1);
		unsigned long next ;
		result = ioctl( fbDev, SM501_GET_SYNCCOUNT, &next );
		if( 0 != result )
			printf( "Error reading next count: %m\n" );

		printf( "%u/%u - %d\n", startCount, next, next-startCount );

		close( fbDev );
	}
	else {
		printf( "Error opening fb device\n" );
	}
	return 0 ;
}

