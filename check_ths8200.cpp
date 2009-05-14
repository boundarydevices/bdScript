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

static unsigned char getByte(int dev, unsigned char reg )
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
		exit(-1);
	} else {
	}
        return buf[4];
}

#include <string.h>
#include <ctype.h>
#include "physMem.h"

#define VENC_BASE 0x01c72000
#define VENC_HINT 0x01c72418
#define VENC_VINT 0x01c72424
#define PAGE_SIZE 4096

inline unsigned long readLong(physMem_t &pm, unsigned long reg)
{
   unsigned offs = reg-VENC_BASE ;
   if( offs >= PAGE_SIZE ){
      fprintf( stderr, "Invalid register 0x%x\n", reg );
      exit(-1);
   }
   void const *p = (char *)pm.ptr() + offs ;
   unsigned long rval ;
   memcpy(&rval,p,sizeof(rval));
   return rval ;
}

#define READLONG(addr)  *((unsigned long *)

int main(void)
{
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
      
   physMem_t venc_regs( VENC_BASE, PAGE_SIZE, O_RDWR );
   if( !venc_regs.worked() ){
      fprintf(stderr, "Error mapping venc regs\n" );
      return -1 ;
   }

   unsigned venc_hint = readLong(venc_regs,VENC_HINT);
   unsigned venc_vint = readLong(venc_regs,VENC_VINT);

   unsigned long ths_hint = (((unsigned)getByte(dev,0x34)) << 8 )
                          | getByte(dev,0x35);
   unsigned long ths_vint = (((((unsigned)getByte(dev,0x39))>>4)&7) << 8 )
                          | getByte(dev,0x3a);

   printf( "hint 0x%08lx, vint 0x%08lx\n", venc_hint, venc_vint );
   printf( "hint 0x%08lx, vint 0x%08lx\n", ths_hint, ths_vint );
   
   if( (venc_hint != (ths_hint-1))
       ||
       (venc_vint != (ths_vint-1)) ){
      fprintf( stderr, "Invalid horizontal or vertical resolutions in THS8200\n" );
      return -1 ;
   }
   return 0 ;
}
