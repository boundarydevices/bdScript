/*
 * Program mtdInfo.cpp
 *
 * This program will dump the MTD information about
 * the specified device to stdout.
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mount.h>
#ifndef KERNEL_2_4
#include <linux/types.h>
#include <mtd/mtd-user.h>
#else
#include <linux/mtd/mtd.h>
#endif

int main( int argc, char const * const argv[] )
{
   if( 1 < argc ){
      char const *devName = argv[1];
      int fd = open( devName, O_RDWR );
      unsigned offset, maxSize ;
      if( 0 <= fd ){
         mtd_info_t meminfo;
         if( ioctl( fd, MEMGETINFO, (unsigned long)&meminfo) == 0)
         {
            printf( "flags       0x%lx\n", meminfo.flags ); 
            printf( "size        0x%lx\n", meminfo.size ); 
            printf( "erasesize   0x%lx\n", meminfo.erasesize ); 
            printf( "oobsize     0x%lx\n", meminfo.oobsize ); 
            printf( "ecctype     0x%lx\n", meminfo.ecctype ); 
            printf( "eccsize     0x%lx\n", meminfo.eccsize ); 
            printf( "numSectors  0x%lx\n", meminfo.size / meminfo.erasesize );
         }
         else
            fprintf( stderr, "%s: GETINFO %m\n", devName );
      }
      else
         fprintf( stderr, "%s: %m\n", devName );
   }
   else
      fprintf( stderr, "Usage: %s /dev/mtd/3\n", argv[0] );

   return 0 ;
}
