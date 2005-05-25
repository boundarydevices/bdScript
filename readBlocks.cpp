/*
 * Program readBlocks.cpp
 *
 * This program will read and perform a hex 
 *
 *
 * Change History : 
 *
 * $Log: readBlocks.cpp,v $
 * Revision 1.1  2005-05-25 16:25:52  ericn
 * -initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include "hexDump.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#define BLKSIZE 512

int main( int argc, char const * const argv[] )
{
   int const fd = open( "/dev/mmcblk0", O_RDONLY );
   if( 0 <= fd )
   {
      unsigned long blkNum = ( 1 < argc ) ? strtoul( argv[1], 0, 0 ) : 0 ;
      unsigned long count = ( 2 < argc ) ? strtoul( argv[2], 0, 0 ) : 1 ;

      printf( "mmc device opened\n" );
      int result = lseek( fd, blkNum*BLKSIZE, SEEK_SET );
      if( 0 <= result )
      {
         for( unsigned i = 0 ; i < count ; i++ )
         {
            unsigned char data[512];
            int numRead = read( fd, data, sizeof( data ) );
            if( 0 < numRead )
            {
               hexDumper_t dumpRx( data, numRead );
               while( dumpRx.nextLine() )
                  printf( "%s\n", dumpRx.getLine() );
            }
            else
               perror( "read" );
         }
      }
      else
         perror( "seek" );
      close( fd );
   }
   else
      perror( "/dev/mmcblk0" );
   
   return 0 ;
}
