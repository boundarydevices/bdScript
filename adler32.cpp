/*
 * Program adler32.cpp
 *
 * This program will perform an adler32() on one or more
 * files (Adler32 is like a fast CRC using the algorithm 
 * defined by Mark Adler as a part of zlib).
 *
 *
 * Change History : 
 *
 * $Log: adler32.cpp,v $
 * Revision 1.1  2004-03-17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 * Copyright Boundary Devices, Inc. 2004
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
            uLong a = adler32( 0, (unsigned char const *)fIn.getData(), fIn.getLength() );
            printf( "adler32: 0x%08lx\n", a );
         }
         else
            perror( fileName );
      }
   }
   else
      fprintf( stderr, "Usage: %s fileName [fileName...]\n", argv[0] );

   return 0 ;
}
