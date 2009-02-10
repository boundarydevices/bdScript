/*
 * Program sm501poke.cpp
 *
 * This program pokes a longword value into 
 * SM-501 RAM.
 *
 *
 * Change History : 
 *
 * $Log: sm501poke.cpp,v $
 * Revision 1.3  2009-02-10 00:29:01  ericn
 * allow other fb devs (use FBDEV environment variable)
 *
 * Revision 1.2  2007-08-08 17:12:00  ericn
 * -[sm501] /dev/fb0 not /dev/fb/0
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#define irqreturn_t int
#include <linux/sm501-int.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <string.h>

static char const *devName = "/dev/fb0" ;

int main( int argc, char const * const argv[] )
{
	if( 1 < argc ){
		unsigned long offset = strtoul(argv[1], 0, 0 );
      if( 0 == ( offset & 3 ) )
      {
                if( getenv( "FBDEV" ) ){
                   devName = getenv( "FBDEV" );
		   printf( "opening device %s\n", devName );
		}
		int const fbDev = open( devName, O_RDWR );
   		if( 0 <= fbDev ) {
            struct fb_fix_screeninfo fixed_info;
            int err = ioctl( fbDev, FBIOGET_FSCREENINFO, &fixed_info);
            if( 0 == err )
            {
               if( offset < fixed_info.smem_len-3 )
               {
                  void *mem = mmap( 0, fixed_info.smem_len, PROT_WRITE|PROT_WRITE, MAP_SHARED, fbDev, 0 );
                  if( MAP_FAILED != mem )
                  {
                     void *addr = ((char *)mem) + offset ;
                     unsigned long oldValue ;
                     memcpy( &oldValue, addr, sizeof(oldValue) );
                     printf( "offset[%08lx] == %08lx\n", offset, oldValue );
                     if( 2 < argc ) {
                        unsigned long value = strtoul(argv[2], 0, 0 );
                        memcpy( addr, &value, sizeof(value) );
                     }
                     if( 0 != munmap( mem, fixed_info.smem_len ) )
                        perror("munmap: fb");
                  }
                  else
                     perror( "mmap" );
               }
               else
                  fprintf( stderr, "out of range: 0..%u\n", fixed_info.smem_len-4 );
            }
            else
               perror( "FBIOGET_FSCREENINFO" );
   			close( fbDev );
   		}
   		else
   			perror( "/dev/fb0" );
   		printf( "\n" );
      }
      else
         fprintf( stderr, "Only longword aligned addresses are allowed\n" );
	}
	else
		fprintf( stderr, "Usage: %s offset [newvalue]\n", argv[0] );
	return 0 ;
}
