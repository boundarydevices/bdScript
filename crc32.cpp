/*
 * Program adler32.cpp
 *
 * This program will perform an crc32() on one or more files.
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include <stdio.h>
#include "memFile.h"
#include <zlib.h>

int main( int argc, char const * const argv[] )
{
   if( 1 < argc )
   {
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         char const *fileName = argv[arg];
         memFile_t fIn( fileName );
         if( fIn.worked() )
         {
            printf( "%s: %lu bytes...", fileName, fIn.getLength() );
            uLong crc = crc32( 0, (unsigned char const *)fIn.getData(), fIn.getLength() );
            printf( "crc32: 0x%08lx\n", crc );
         }
         else
            perror( fileName );
      }
   }
   else
      fprintf( stderr, "Usage: %s fileName [fileName...]\n", argv[0] );

   return 0 ;
}
