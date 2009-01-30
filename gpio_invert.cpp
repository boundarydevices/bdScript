#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
// #include <linux/pxa-gpio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>

#define MAX_INVERT_BITS    128
#define INVERT_BYTES       (MAX_INVERT_BITS/8)

typedef unsigned char __u8 ;

#define BASE_MAGIC 250
#define GPIO_GET_INVERT _IOR(BASE_MAGIC, 0x04, __u8[INVERT_BYTES]) // returns bit mask of pins to invert
#define GPIO_SET_INVERT _IOW(BASE_MAGIC, 0x05, __u8[INVERT_BYTES]) // accepts bit mask of pins to invert

static inline void setBit(unsigned char *bytes,unsigned bit)
{
	bytes[(bit/8)] |= (1<<(bit&7));
}

static inline void clearBit(unsigned char *bytes,unsigned bit)
{
	bytes[(bit/8)] &= ~(1<<(bit&7));
}

static inline bool isSet(unsigned char const *bytes,unsigned bit)
{
	return (0 != (bytes[(bit/8)] & (1<<(bit&7))));
}

static void showInverted(unsigned char const *bytes)
{
   for( unsigned i = 0 ; i < MAX_INVERT_BITS ; i++ )
   {
      if(isSet(bytes,i)){
         printf( "%u ", i );
      }
   }
   printf( "\n" );
}

int main( int argc, char const * const argv[] )
{
	if( 2 <= argc ){
		int fd = open( argv[1], O_RDWR );
		if( 0 <= fd ){
                        static unsigned char invertBytes[INVERT_BYTES];
			int rval = ioctl(fd,GPIO_GET_INVERT,invertBytes);
			if( 0 == rval ){
                                printf( "old: " ); showInverted(invertBytes);
				for( int arg = 2 ; arg < argc ; arg++ ){
					char const *pin = argv[arg];
					int clear = 0 ;
					if( ('-' == *pin) || ('!' == *pin) ){
						clear = 1 ;
						pin++ ;
					}
					char *endptr ;
					unsigned long pinNum = strtoul(pin,&endptr,0);
					if((pinNum < MAX_INVERT_BITS)&&(0 != endptr)&&(0==*endptr)){
						if( clear )
							clearBit(invertBytes,pinNum);
						else
							setBit(invertBytes,pinNum);
					} else {
						fprintf( stderr, "Invalid pin %s\n", argv[arg]);
						return -1 ;
					}
				}
				if( 2 < argc ){
                                        rval = ioctl(fd,GPIO_SET_INVERT,invertBytes);
					if( rval ){
						perror( "GPIO_SET_INVERT" );
						return -1 ;
					}
                                        printf( "new: " ); showInverted(invertBytes);
				} // set pin invert bits
			}
			else
				fprintf( stderr, "GPIO_GET_INVERT:%m\n" );

			close(fd);
		}
		else
			perror( argv[1] );
	}
	else
		fprintf( stderr, "Usage: %s devname [pin [..pin]]\n", argv[0] );
	return 0 ;
}
