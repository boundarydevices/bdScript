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

#define MAX_REG 0x82

static char const usage[] = {
        "Usage: ths8200_reg register [value]\n"
        "   register can be either a single register (0xNN) or two registers\n"
        "   (0xNN+0xNN) MSB-first\n"
};

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

static bool getByte(int dev, unsigned char reg, unsigned char &value)
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
		return false ;
	} else {
           value = buf[4];
           return true ;
	}
}

#include <string.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
   if( 2 <= argc ){
      int dev;
      int adapter_nr = 1; /* probably dynamically determined */
      char filename[20];
      sprintf(filename,"/dev/i2c-%d",adapter_nr);
      if ((dev = open(filename,O_RDWR)) < 0) {
         printf("error opening %s(%s)\n",filename,strerror(errno));
         return -1 ;
      }
      
      if (ioctl(dev,I2C_SLAVE,addr) < 0) {
           printf("error setting slave address %i(%s)\n",addr,strerror(errno));
           return -1 ;
      }

      printf( "registers %s\n", argv[1] );
      unsigned num_regs = 0 ;
      unsigned value = 0 ;
      char *reg_str ;
      char *in_str = argv[1];
      while( 0 != ( reg_str = strtok(in_str,"+:.") ) ){
         char *endptr ;
         unsigned regnum = strtoul(reg_str,&endptr,0);
         if(endptr && *endptr){
           fprintf( stderr, "Invalid register %s\n", reg_str );
           return -1 ;
         }
         if( regnum > MAX_REG ){
           fprintf( stderr, "Invalid register %s\n", reg_str );
           return -1 ;
         }
         unsigned char regVal ;
         if( !getByte(dev,regnum,regVal) ){
           fprintf( stderr, "Error reading register 0x%02x\n", regnum );
           return -1 ;
         }
         
         printf( "reg %s == 0x%02x\n", reg_str, regVal );
         value = (value << 8) | regVal ;
         
         in_str = 0 ;
      }
      printf( "registers[%s] == 0x%x\n", argv[1], value );
      
      if( 3 <= argc ){
      printf( "    -> values %s\n", argv[2] );
      }
   }
   else
          fprintf( stderr, usage );
   
   return 0 ;
}
