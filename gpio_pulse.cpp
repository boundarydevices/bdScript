#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>

#define BASE_MAGIC 250
typedef unsigned long __u32 ;
#define PULSELOW(duration)	((duration)&0x7fffffff)
#define PULSEHIGH(duration)	(0x80000000|((duration)&0x7fffffff))
#define GPIO_PULSE _IOW(BASE_MAGIC, 0x06, __u32)

int main( int argc, char const * const argv[] )
{
	if( 3 < argc ){
		int fd = open( argv[1], O_RDWR );
		if( 0 <= fd ){
			unsigned long dir = strtoul(argv[2],0,0);
			if( 1 >= dir ){
				unsigned long duration = strtoul(argv[3],0,0);
				if( (0 < duration) && (0x7fffffff >= duration) ){
					unsigned long param = (dir << 31)|duration ;
					int rval = ioctl(fd,GPIO_PULSE,&param);
					sleep(1+(duration/100));
				}
				else
					fprintf( stderr, "Invalid duration, specify 1..0x7fffffff in jiffies\n" );
			}
			else
				fprintf( stderr, "Invalid direction %s (use 0 or 1)\n", argv[2] );

			close(fd);
		}
		else
			perror( argv[1] );
	}
	else
		fprintf( stderr, "Usage: %s devname dir(0|1) duration(jiffies)\n", argv[0] );
	return 0 ;
}
