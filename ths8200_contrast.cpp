#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>

#define PORTA	0x05
#define TRISA	0x85

#define PORTC	0x07
#define TRISC	0x87

#define WPUA	0x95

#define ADRESH	0x1e
#define ADRESL	0x9e

#define ADCON0	0x1f
#define ADCON1	0x9f

#define ANSEL	0x91

static unsigned char const lsbRegs[] = {
	0x4c, 0x4d, 0x4e
};

static unsigned char const msbRegs[] = {
	0x4a, 0x4b, 0x4b
};

static unsigned char const msbNibs[] = {
	0, 0, 1
};

static char const usage[] = {
        "Usage: contrast [0..100] (in percent)\n"
};

unsigned const RANGE = 1023 ;
unsigned const DEFAULT_CONTRAST = 63 ;

#define DEVBASE  "/sys/class/i2c-adapter/i2c-1/1-0021/"
#define DEVWRITE DEVBASE "write"
#define DEVREAD DEVBASE "read"

static void setByte(unsigned char reg,unsigned char lsb)
{
	int fdWrite = open(DEVWRITE, O_WRONLY);
	if( 0 > fdWrite ){
		perror( DEVWRITE );
		exit(1);
	}
	char buf[32];
	snprintf(buf,sizeof(buf), "%02x %02x", reg, lsb );
	if ( write(fdWrite,buf,5) != 5) {
		printf("i2c write error(%s), reg=0x%02x\n",strerror(errno),reg);
		exit(1);
	}
	close(fdWrite);
}

static unsigned char getByte(unsigned char reg)
{
	int fdWrite = open(DEVWRITE, O_WRONLY);
	if( 0 > fdWrite ){
		perror( DEVWRITE );
		exit(1);
	}
	char cReg[8];
	snprintf( cReg, sizeof(cReg), "%02x", reg );
	if( 2 != write(fdWrite, cReg, 2) ){
		perror( "WRITE:" DEVWRITE );
		exit(1);
	}
	close(fdWrite);
	
	int fdRead = open(DEVREAD, O_RDONLY);
	if( 0 > fdRead ){
		perror( DEVREAD );
		exit(1);
	}
	int numRead = read(fdRead, cReg, 2 );
	if( 2 != numRead ){
                perror( "READ:" DEVREAD );
		exit(1);
	}
	close(fdRead);
	cReg[2] = '\0' ;
	unsigned val ;
	if( 1 != sscanf( cReg, "%02x", &val ) ){
		perror( "DATA:" DEVREAD );
		exit(1);
	}
	return (unsigned char) val ;
}

int main(int argc, char *argv[])
{
	if( 2 <= argc ){
		unsigned percent ;
		if( 0 == strncasecmp("def",argv[1],3) ){
			percent = DEFAULT_CONTRAST ;
			printf( "Using default value of %u\n", percent );
		}
		else {
			percent = strtoul(argv[1],0,0);
			printf( "set contrast to %u%%\n", percent );
		}
		if( 100 >= percent ){
			int i=0;
			int reg = 0;
			int val = 0;
		
			// Turn on filtering
			unsigned char regval = getByte(0x4f);
			regval |= 0xc0 ;
			setByte(0x4f,regval);

                        regval = getByte(0x4a);
			regval |= 0x80 ;
			setByte(0x4a,regval);

			unsigned value = (RANGE*percent)/100 ;
			for( unsigned i = 0 ; i < 3 ; i++ ){
				setByte(lsbRegs[i],(unsigned char)(value&0xff));
				unsigned char mask = msbNibs[i] ? 0x8f : 0xf8 ;
	  			unsigned char prev = getByte(msbRegs[i]);
				prev &= mask ;

				unsigned char msb = value >> 8 ;
				msb &= 0x07 ;
				msb <<= 4*msbNibs[i];

				prev |= msb ;
				setByte(msbRegs[i],prev);
			}
			printf( "changed contrast to %u%%\n", percent );
			return 0;
		}
		else
			fprintf( stderr, usage );
	}
	else
		fprintf( stderr, usage );

	return 0 ;
}
