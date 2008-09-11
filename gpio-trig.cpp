#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/gpio-trig.h>
#include <ctype.h>

int main( int argc, char const * const argv[] ){
	if( 4 <= argc ){
		unsigned trigNum = strtoul(argv[2],0,0);
		if( trigNum && trigNum < 256 ){
			struct trigger_t trig ;
			trig.trigger_pin = trigNum ;
			trig.rising_edge = ('r' == tolower(*argv[3]));
			trig.num_pins = 0 ;
			for( int arg = 4 ; arg < argc ; arg++ ) {
				unsigned pin = strtoul(argv[arg],0,0);
				if( pin && (pin < 256) ){
					trig.pins[trig.num_pins++] = pin ;
				}
				else {
					fprintf( stderr, "Invalid pin <%s>\n", argv[arg] );
					return -1; 
				}
			}
			int fdIn = open( argv[1], O_RDONLY );
			if( 0 <= fdIn ){
				int result = ioctl( fdIn, GPIO_TRIG_CFG, &trig );
				if( 0 == result ){
					printf( "configured GPIO trigger\n" );
					unsigned char inbuf[80];
					int numRead ;
					unsigned totalRead = 0 ;
					while( 0 < (numRead = read(fdIn,inbuf,sizeof(inbuf))) ){
						for( int i = 0 ; i < numRead ; i++ ){
							printf( "%5u:%5u: %02x\n", totalRead, i, inbuf[i] );
						}
						totalRead += numRead ;
					}

				}
				else
					perror( "GPIO_TRIG_CFG" );
				close(fdIn );
			}
			else
				perror( argv[1] );
		} else
			printf( "Invalid trigger <%s>\n", argv[2] );
	} else
		fprintf( stderr, "Usage: %s /dev/gpio-trig  triggerPin Rising|falling [...otherPins]\n", argv[0] );
	return 0 ;
}
