/*
 * Module img4444.cpp
 *
 * This module defines the imgTo4444() routine and
 * the image4444_t class as declared in img4444.h
 *
 *
 * Change History : 
 *
 * $Log: img4444.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "img4444.h"
#include <string.h>

void imgTo4444( unsigned short const *inPixels,
                unsigned              width,
                unsigned              height,
                unsigned char const  *inAlpha,
                unsigned short       *out4444,
                unsigned              outStride )
{
   for( unsigned y = 0 ; y < height ; y++ ){
      for( unsigned x = 0 ; x < width ; x++ ){
         unsigned short const in = *inPixels++ ;
         unsigned const b = (in >> 1) & 15 ;
         unsigned const g = (in >> 7) & 15 ;
         unsigned const r = (in >> 12);
         unsigned const a = (0!=inAlpha) 
                            ? ((*inAlpha++) >> 4)
                            : 0x0f ;
         unsigned short rgba = (a<<12)|(r<<8)|(g<<4)|b ;
         out4444[x] = rgba ;
      }
      out4444 += outStride ;
   }
}
                
#ifdef MODULETEST

#include "imgFile.h"
#include "fbImage.h"
#include <stdio.h>
#include "fbDev.h"
#include "sm501alpha.h"

int main( int argc, char const * const argv[] )
{
   if( 1 < argc ){
      fbDevice_t &fb = getFB();
      unsigned x = 0 ;
      unsigned y = 0 ;

      sm501alpha_t &alphaLayer = sm501alpha_t::get(sm501alpha_t::rgba4444);

      for( int arg = 1 ; arg < argc ; arg++ ){
         char const *fileName = argv[arg];
         image_t image ;
         if( imageFromFile( fileName, image ) )
         {
            fbImage_t fbi( image, fbi.rgba4444 );

            alphaLayer.draw4444( (unsigned short *)fbi.pixels(),
                                 x, y, fbi.width(), fbi.height() );
            printf( "displayed %s", fileName );
            char inBuf[256];
            fgets( inBuf, sizeof(inBuf), stdin );
         }
         else
            perror( fileName );
      }
   }
   else
      fprintf( stderr, "Usage: img4444 fileName [fileName...]\n" );
   return 0 ;
}

#endif
