/*
 * Module imgTransparent.cpp
 *
 * This module defines the imgTransparent()
 * routine as declared in imgTransparent.h
 *
 *
 * Change History : 
 *
 * $Log: imgTransparent.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "imgTransparent.h"
#include <string.h>

void imgTransparent( image_t       &img,
                     unsigned char  maxOpacity )
{
   if( 0 == img.alpha_ ){
      unsigned numBytes = img.width_*img.height_ ;
      if( numBytes ){
         unsigned char *bytes = new unsigned char [numBytes];
         img.alpha_ = bytes ;
         memset( bytes, maxOpacity, numBytes );
      }
   }
   else {
      if( 255 != maxOpacity ){
         unsigned char *outAlpha = (unsigned char *)img.alpha_ ;
         for( unsigned y = 0 ; y < img.height_ ; y++ ){
            for( unsigned x = 0 ; x < img.width_ ; x++ ){
               unsigned a = *outAlpha ;
               a *= maxOpacity ;
               a /= 255 ;
               *outAlpha++ = (unsigned char)a ;
            }
         }
      } // or what's the point?
   }
}

#ifdef MODULETEST
#include <stdio.h>
#include "fbDev.h"
#include <stdlib.h>
#include "imgFile.h"
#include "imgToPNG.h"

int main( int argc, char const * const argv[] )
{
   if( 3 < argc )
   {
      image_t image ;
      if( imageFromFile( argv[1], image ) )
      {
         unsigned max = strtoul( argv[2], 0, 0 );

         imgTransparent( image, max );
         getFB().render( 0, 0, 
                         image.width_, image.height_, 
                         (unsigned short *)image.pixData_,
                         (unsigned char *)image.alpha_ );
         printf( "%u x %u\n", image.width_, image.height_ );
		
         void const *outData ;
   		unsigned outSize ;
   		if( imageToPNG( image, outData, outSize ) )
   		{
   			printf( "%u bytes of output\n", outSize );
   			char const *outFile = argv[3];
   			FILE *fOut = fopen( outFile, "wb" );
   			if( fOut )
   			{
   				fwrite( outData, 1, outSize, fOut );
   				fclose( fOut );
   				printf( "%u bytes written to %s\n", outSize, outFile );
   			}
   			else
   				perror( outFile );
   			free((void *)outData);
   		}
   		else
   			printf( "Error converting image\n" );

      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: imgFile fileName maxOpacity outFile\n" );
   return 0 ;
}   
#endif
