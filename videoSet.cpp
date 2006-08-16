/*
 * Program videoSet.cpp
 *
 * This is a test program which allows setting
 * of frame-buffer (video) data from the command line.
 *
 *
 * Change History : 
 *
 * $Log: videoSet.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include <stdio.h>
#include "fbDev.h"
#include <stdlib.h>

int main( int argc, char const * const argv[] )
{
   printf( "Hello, %s\n", argv[0] );
   fbDevice_t &fb = getFB();

   if( 3 < argc )
   {
      unsigned long offset = strtoul( argv[1],0,0 );
      unsigned long numPixels = strtoul( argv[2],0,0 );
      unsigned short pixValue = strtoul(argv[3],0,0);

      unsigned short *vidMem = (unsigned short *)
                               ((unsigned long)fb.getMem()+offset);
      for( unsigned i = 0 ; i < numPixels ; i++ )
         *vidMem++ = pixValue ;
   }
   else
      fprintf( stderr, "Usage: videoSet offset(bytes) pixelCount value\n" );

   return 0 ;
}
