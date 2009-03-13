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
//#include <asm/types.h>
typedef unsigned short __u16;
typedef unsigned char __u8;
#define I2C_RDWR  0x0707  /* Combined R/W transfer (one stop only)*/
#define I2C_SLAVE 0x0703
struct i2c_msg {
	__u16 addr;     /* slave address */
	__u16 flags;
#define I2C_M_TEN			0x10    /* we have a ten bit chip address */
#define I2C_M_RD			0x01
#define I2C_M_NOSTART		0x4000
#define I2C_M_REV_DIR_ADDR	0x2000
#define I2C_M_IGNORE_NAK	0x1000
#define I2C_M_NO_RD_ACK		0x0800
#define I2C_M_RECV_LEN		0x0400 /* length will be first received byte */
	__u16 len;              /* msg length */
	__u8 *buf;              /* pointer to msg data */
};

static int const addr = 0x42>>1; /* The I2C address, low 7 bits are the address bits */

#include <linux/i2c-dev.h>
//typedef signed int s32;
//typedef unsigned char u8;
//struct i2c_client;
//extern s32 i2c_smbus_read_byte_data(struct i2c_client * client, u8 command);
//#include <linux/device.h>
//#include <linux/i2c.h>

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

static void setByte(int dev, unsigned char reg,unsigned char lsb)
{
	__u8 buf[32];
	buf[0] = reg;
	buf[1] = lsb;
	if ( write(dev,buf,2) != 2) {
		printf("i2c write error(%s), reg=0x%02x\n",strerror(errno),reg);
		exit(1);
	}
}

static unsigned char getByte(int dev, unsigned char reg)
{
	__u8 buf[32];
	buf[0] = reg;

	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data msgset;

	msgs[0].addr = addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = buf;

	msgs[1].addr = addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = &buf[4];

	msgset.msgs = msgs;
	msgset.nmsgs = 2;
	if (ioctl(dev,I2C_RDWR,&msgset)<0) {
		printf("I2C_RDWR(%s), reg=0x%02x addr=0x%02x\n", strerror(errno), reg, addr);
		exit(1);
	} else {
	}

	return buf[4];
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
			int dev;
			int i=0;
			int adapter_nr = 1; /* probably dynamically determined */
			char filename[20];
			int reg = 0;
			int val = 0;
		
			sprintf(filename,"/dev/i2c-%d",adapter_nr);
			if ((dev = open(filename,O_RDWR)) < 0) {
				printf("error opening %s(%s)\n",filename,strerror(errno));
				return -1 ;
			}
		
			if (ioctl(dev,I2C_SLAVE,addr) < 0) {
				printf("error setting slave address %i(%s)\n",addr,strerror(errno));
				return -1 ;
			}
		
			// Turn on filtering
			unsigned char regval = getByte(dev,0x4f);
			regval |= 0xc0 ;
			setByte(dev,0x4f,regval);

                        regval = getByte(dev,0x4a);
			regval |= 0x80 ;
			setByte(dev,0x4a,regval);

			unsigned value = (RANGE*percent)/100 ;
			for( unsigned i = 0 ; i < 3 ; i++ ){
				setByte(dev,lsbRegs[i],(unsigned char)(value&0xff));
				unsigned char mask = msbNibs[i] ? 0x8f : 0xf8 ;
	  			unsigned char prev = getByte(dev,msbRegs[i]);
				prev &= mask ;

				unsigned char msb = value >> 8 ;
				msb &= 0x07 ;
				msb <<= 4*msbNibs[i];

				prev |= msb ;
				setByte(dev,msbRegs[i],prev);
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
