#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>

typedef unsigned short __u16 ;
typedef unsigned char  __u8 ;
typedef long int       __s32 ;
#include "i2c-dev.h"

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
   int addr = 0x50; /* The I2C address */

   if (ioctl(file, I2C_SLAVE, addr) < 0) {
      /* ERROR HANDLING; you can check errno to see what went wrong */
      perror( "I2C_SLAVE" );
      exit(1);
   }

   for( int i = 1 ; i < argc ; i++ ){
      __u8 regnum = strtoul(argv[i],0,0); /* Device register to access */
      __s32 res;
      char buf[10];
      
      /* Using SMBus commands */
      res = i2c_smbus_read_word_data(file, regnum);
      if (res < 0) {
          /* ERROR HANDLING: i2c transaction failed */
         perror( "read_word_data" );
      } else {
       /* res contains the read word */
         printf( "register[0x%02x] == %04x\n", regnum, res );
      }
   }

   return 0 ;
}
