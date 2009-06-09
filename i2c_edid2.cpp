#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>

typedef unsigned short __u16 ;
typedef unsigned char  __u8 ;
typedef long int       __s32 ;
#include "i2c-dev.h"

#define DDC_ADDR 0x50
#define EDID_LENGTH				0x80

int main(int argc, char const * const argv[]){
   int file;
   int adapter_nr = 0; /* probably dynamically determined */
   char filename[20];

   snprintf(filename, 19, "/dev/i2c-%d", adapter_nr);
   file = open(filename, O_RDWR);
   if (file < 0) {
      /* ERROR HANDLING; you can check errno to see what went wrong */
      perror(filename);
      return -1 ;
   }
   
   printf( "opened %s\n", filename );
   int addr = DDC_ADDR; /* The I2C address */

   if (ioctl(file, I2C_SLAVE, addr) < 0) {
      /* ERROR HANDLING; you can check errno to see what went wrong */
      perror( "I2C_SLAVE" );
      exit(1);
   }

   unsigned char edid_data[256];
   __s32 res = i2c_smbus_read_block_data(file,0,edid_data);
   if( 0 < res ){
       printf( "read %u bytes of data\n", res );
   }
   else
       perror( "i2c_smbus_read_block_data" );

/*
   struct i2c_rdwr_ioctl_data msg_rdwr;
   unsigned char start = 0x0;
   struct i2c_msg msgs[] = {
          {
                  DDC_ADDR,
                  0,
                  1,
                  (char *)&start,
          }, {
                  DDC_ADDR,
                  I2C_M_RD,
                  EDID_LENGTH,
                  (char *)edid_data,
          }
   };
   struct i2c_msg             i2cmsg;
   int i;
   msg_rdwr.msgs = msgs;
   msg_rdwr.nmsgs = 2;
   if ((i = ioctl(file, I2C_RDWR, &msg_rdwr)) < 0)
      perror("I2C_RDWR");
   else
      printf( "read %u bytes of data\n", i );
*/
                                                                                                                                                                                                          
   return 0 ;
}
