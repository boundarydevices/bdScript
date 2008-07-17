/*
 * countColors.cpp
 * 
 * Program to count Y, U, and V values in one or more
 * YUV files.
 *
 * Prints output of each file as well as totals for all
 * files.
 *
 */

#include <stdio.h>
#include "memFile.h"
#include <zlib.h>
#include <string.h>
#include <map>

typedef std::map<unsigned,unsigned> valueCounter_t ;

int main( int argc, char const * const argv[] )
{
   valueCounter_t global ;
   for( unsigned arg = 1 ; arg < argc ; arg++ ){
      char const * const fileName = argv[arg];
      memFile_t fIn( fileName );
      if( !fIn.worked() ){
         perror( fileName );
         continue;
      }
      valueCounter_t local ;
      unsigned numPix = fIn.getLength()*2 / 3 ;

      unsigned char const *yValues = (unsigned char const *)fIn.getData();
      unsigned char const *uValues = yValues[numPix];
      unsigned char const *vValues = uValues[numPix/4];
      for( unsigned i = 0 ; i < numPix ; i++ ){
         unsigned const yValue = yValues[i];
         unsigned const uValue = uValues[i/4];
         unsigned const vValue = vValues[i/4];
         unsigned const yUV = yValue << 
         local.addY( *next++ );
      }
      numPix /= 4 ;
      for( unsigned i = 0 ; i < numPix ; i++ ){
         local.addU( *next++ );
      }
      for( unsigned i = 0 ; i < numPix ; i++ ){
         local.addV( *next++ );
      }
      global += local ;
      printf( "%s: %u, %u, %u\n", fileName, local.numY(), local.numU(), local.numV() );
   }
   printf( "global: %u, %u, %u\n", global.numY(), global.numU(), global.numV() );
   for( unsigned i = 0 ; i < 256 ; i++ ){
//      printf( "Y[%u] == %u\n", i, global.yCount(i) );
   }
   printf( "yRange: %u..%u (%u)\n", global.minY(), global.maxY(), global.maxY()-global.minY() );
   for( unsigned i = 0 ; i < 256 ; i++ ){
//      printf( "U[%u] == %u\n", i, global.uCount(i) );
   }
   printf( "uRange: %u..%u (%u)\n", global.minU(), global.maxU(), global.maxU()-global.minU() );
   for( unsigned i = 0 ; i < 256 ; i++ ){
//      printf( "V[%u] == %u\n", i, global.vCount(i) );
   }
   printf( "vRange: %u..%u (%u)\n", global.minV(), global.maxV(), global.maxV()-global.minV() );
   return 0 ;
}
