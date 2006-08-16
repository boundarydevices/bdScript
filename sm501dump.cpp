/*
 * Program sm501dump.cpp
 *
 * This program allows dumping data from 
 * SM-501 RAM.
 *
 *
 * Change History : 
 *
 * $Log: sm501dump.cpp,v $
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
#include "hexDump.h"

int main( int argc, char const * const argv[] )
{
	if( 1 < argc ){
		unsigned long offset = strtoul(argv[1], 0, 0 );
      if( 0 == ( offset & 3 ) )
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
                     unsigned const blockSize = 256 ;
                     void *addr = ((char *)mem) + offset ;
                     char inBuf[80];
                     do {
                        hexDumper_t dump( addr, blockSize, offset );
                        while( dump.nextLine() )
                           printf( "%s\n", dump.getLine() );
                        addr = (char *)addr + blockSize ;
                        offset += blockSize ;
                     } while( fgets(inBuf,sizeof(inBuf),stdin ) );
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
         fprintf( stderr, "Only longword aligned addresses are allowed\n" );
	}
	else
		fprintf( stderr, "Usage: %s offset [newvalue]\n", argv[0] );
	return 0 ;
}
