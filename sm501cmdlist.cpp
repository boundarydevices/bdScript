/*
 * Program sm501cmdlist.cpp
 *
 * This program reads a set of commands from stdin
 * and executes the corresponding command list.
 *
 * Change History : 
 *
 * $Log: sm501cmdlist.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
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
#include <errno.h>
#include "tickMs.h"

static unsigned long finish_cmd[] = {
   0x80000001,
   0x00000000
};

int main( int argc, char const * const argv[] )
{
	if( 1 < argc ){
		unsigned long offset = strtoul(argv[1], 0, 0 );
      if( 0 == ( offset & 7 ) )
      {
   		int const fbDev = open( "/dev/fb/0", O_RDWR );
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
                     unsigned long const start_offs = offset ;
                     void *addr = ((char *)mem) + offset ;

                     printf( "enter a set of commands in hex, terminate with <ctrl-d>\n" );
                     char inbuf[80];
                     while( fgets( inbuf, sizeof(inbuf), stdin ) )
                     {
                        unsigned long oldValue ;
                        memcpy( &oldValue, addr, sizeof(oldValue) );
                        printf( "offset[%08lx] == %08lx ", offset, oldValue );
                        unsigned long value = strtoul(inbuf,0,0);
                        memcpy( addr, &value, sizeof(value) );
                        printf( " ==> 0x%08lx\n", value );
                        offset += sizeof(offset);
                        addr = (char *)addr + sizeof(offset);
                     }

                     unsigned long size = offset-start_offs ;
                     if( 0 == (size&7) ){
                        memcpy( addr, finish_cmd, sizeof(finish_cmd) );
                        long long start = tickMs();
                        int rval = ioctl( fbDev, SM501_EXECCMDLIST, &start_offs );
                        long long end = tickMs();
                        if( 0 == rval )
                           printf( "command completed in %u ms\n", end-start );
                        else
                           printf( "Error %d(%s)\n", rval, strerror(errno) );
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
   			perror( "/dev/fb/0" );
   		printf( "\n" );
      }
      else
         fprintf( stderr, "Only 8-byte aligned addresses are allowed\n" );
	}
	else
		fprintf( stderr, "Usage: %s offset [newvalue]\n", argv[0] );
	return 0 ;
}
