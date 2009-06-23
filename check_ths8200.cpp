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

char const hexCharSet[] = "0123456789abcdef";
unsigned getByte(char *readfile, int fdwrite, int reg)
{
	int fdread;
	int rval;
	int val;
	char buf[16];
	buf[0] = hexCharSet[(reg >> 4) & 0xf];
	buf[1] = hexCharSet[reg & 0xf];
	buf[2] = 0;
	if (write(fdwrite, buf, 2) != 2) {
		printf("error selecting register %02x(%s)\n", reg, strerror(errno));
		return ~0;
	}
	if ((fdread = open(readfile, O_RDONLY)) < 0) {
		printf("error opening %s(%s)\n", readfile, strerror(errno));
		return ~0;
	}
	rval = read(fdread, buf, 15);
	close(fdread);
	if (rval != 3) {
		printf("error reading register %02x(%s)%i\n", reg, strerror(errno), rval);
		printf("%s\n", buf);
		return ~0;
	}
	rval = sscanf(buf, "%x", &val);
	if (rval != 1) {
		printf("error parsing register %02x(%s)%i\n", reg, strerror(errno), rval);
		return ~0;
	}
	return val;
}

int main(void)
{
   unsigned long ths_hint, ths_vint;
   int fdwrite;
   int adapter_nr = 1; /* probably dynamically determined */
   char filename[80];
   sprintf(filename,"/sys/class/i2c-adapter/i2c-%d/1-0021/write",adapter_nr);
   if ((fdwrite = open(filename,O_WRONLY)) < 0) {
      printf("error opening %s(%s)\n",filename,strerror(errno));
      return -1 ;
   }
   sprintf(filename,"/sys/class/i2c-adapter/i2c-%d/1-0021/read",adapter_nr);

   physMem_t venc_regs( VENC_BASE, PAGE_SIZE, O_RDWR );
   if( !venc_regs.worked() ){
      fprintf(stderr, "Error mapping venc regs\n" );
      return -1 ;
   }

   unsigned venc_hint = readLong(venc_regs,VENC_HINT);
   unsigned venc_vint = readLong(venc_regs,VENC_VINT);

   ths_hint = (getByte(filename, fdwrite, 0x34) << 8) |
   	getByte(filename, fdwrite, 0x35);
   ths_vint = (((getByte(filename, fdwrite, 0x39) >> 4) & 7) << 8 ) |
   	getByte(filename, fdwrite, 0x3a);

   close(fdwrite);
   printf( "VENC:\thint 0x%08lx, vint 0x%08lx\n", venc_hint, venc_vint );
   printf( "THS:\thint 0x%08lx, vint 0x%08lx\n", ths_hint, ths_vint );
   
   if ((venc_hint != (ths_hint-1)) || (venc_vint != (ths_vint-1)) ){
      fprintf( stderr, "Invalid horizontal or vertical resolutions in THS8200\n" );
      return -1 ;
   }
   return 0 ;
}
