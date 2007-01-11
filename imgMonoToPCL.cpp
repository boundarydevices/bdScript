/*
 * Module imgMonoToPCL.cpp
 *
 * This module defines the imgMonoToPCL()
 * routine as declared in imgMonoToPCL.h
 *
 *
 * Change History : 
 *
 * $Log: imgMonoToPCL.cpp,v $
 * Revision 1.1  2007-01-11 21:34:14  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */


#include "imgMonoToPCL.h"
#include "dither.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ESC "\x1b"

static char const imagePreamble[] = {
   ESC "*r3F"         //  set rotation to page orientation
   ESC "*t1200R"      //  set DPI to 600
   ESC "*r%uT"        //  set number of raster lines
   ESC "*r%uS"        //  set number of pixels / line
   ESC "*r1U"         //  simple, black & white palette
   ESC "*r1A"         //  start raster graphics at current position
   ESC "*b0Y"         //  move by 0 lines
   ESC "*b0M"         //  compression mode zero
};

static char const linePreamble[] = {
   ESC "*b%uW"        // how many bytes in this line
};

static char const imageTrailer[] = {
   ESC "*r0C"         // end raster graphics
};

static unsigned const padLength = sizeof(imagePreamble)+sizeof(imageTrailer)+40 ; // 3xmax(%u)

void imgMonoToPCL( image_t const &img,
                   char         *&outPCL,
                   unsigned      &outLen )
{
   dither_t dither( (unsigned short const *)img.pixData_, img.width_, img.height_ );
   unsigned const pixelBytesPerLine = (img.width_+7)/8 ;
   char rowPreamble[sizeof(linePreamble)+10];
   unsigned const rpLen = snprintf( rowPreamble, sizeof(rowPreamble), linePreamble, pixelBytesPerLine );
   unsigned const rowLen = rpLen + pixelBytesPerLine ;
   char preamble[sizeof(imagePreamble)+30];
   unsigned const preambleLen = snprintf( preamble, sizeof(preamble), imagePreamble, img.height_, img.width_ );
   unsigned const memSize = preambleLen + (rowLen*img.height_)+sizeof(imageTrailer);
   fprintf( stderr, "image: %ux%u, %u bytes\n", img.width_, img.height_, memSize );
   outPCL = (char *)malloc(memSize+1);
   outPCL[memSize] = 0 ;
   char *nextOut = outPCL ;
   memcpy( nextOut, preamble, preambleLen );
   nextOut += preambleLen ;
   for( unsigned i = 0 ; i < img.height_ ; i++ )
   {
      memcpy( nextOut, rowPreamble, rpLen );
      nextOut += rpLen ;
      unsigned char mask = 0x80 ;
      unsigned char *pixOut = (unsigned char *)nextOut ;
      *pixOut = 0 ;
      for( unsigned j = 0 ; j < img.width_ ; j++ )
      {
         if( dither.isWhite(j,i) )
            *pixOut &= ~mask ;
         else
            *pixOut |= mask ;
         mask >>= 1 ;
         if( 0 == mask ){
            pixOut++ ;
            *pixOut = 0 ;
            mask = 0x80 ;
         }
      }
      nextOut += pixelBytesPerLine ;
   }
   memcpy( nextOut, imageTrailer, sizeof(imageTrailer));
   outLen = (nextOut+sizeof(imageTrailer))-outPCL ;
}


#ifdef MODULETEST
#include "imgFile.h"
#include <stdlib.h>
#include "bdGraph/Scale16.h"

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      image_t image ;
      if( imageFromFile( argv[1], image ) )
      {
         unsigned w = image.width_ ;
         if( 3 <= argc )
            w = strtoul( argv[2], 0, 0 );

         unsigned h = image.height_ ;
         if( 4 <= argc )
            h = strtoul( argv[3], 0, 0 );
         
         if( ( w != image.width_ )
             ||
             ( h != image.height_ ) ){
            unsigned const pixBytes = w*h*sizeof( short );
            void *const pixMem = new unsigned short [w*h];
            Scale16::scale( (unsigned short *)pixMem, w, h, (unsigned short *)image.pixData_, image.width_,image.height_, 0,0,image.width_,image.height_);
            
            image.unload();
            delete [] ((unsigned short *)image.pixData_);
            image.pixData_ = pixMem ;
            image.width_ = w ;
            image.height_ = h ;
         }

         char *outData ;
         unsigned outLen ;
         imgMonoToPCL( image, outData, outLen );

         fwrite( outData, outLen, 1, stdout );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s fileName [w [h]]\n", argv[0] );
   return 0 ;
}

#endif
