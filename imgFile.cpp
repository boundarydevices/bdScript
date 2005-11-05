/*
 * Module imgFile.cpp
 *
 * This module defines the imageFromFile() routine
 * as declared in imgFile.h
 *
 *
 * Change History : 
 *
 * $Log: imgFile.cpp,v $
 * Revision 1.5  2005-11-05 20:23:31  ericn
 * -add standalone utility
 *
 * Revision 1.4  2005/08/12 03:31:42  ericn
 * -include <string.h>
 *
 * Revision 1.3  2004/05/02 14:05:08  ericn
 * -use PNG by default
 *
 * Revision 1.2  2003/10/19 17:02:36  ericn
 * -added GIF support
 *
 * Revision 1.1  2003/10/18 19:16:16  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "imgFile.h"
#include "memFile.h"
#include "imgJPEG.h"
#include "imgPNG.h"
#include "imgGIF.h"
#include <string.h>

#define __USEJPEG__ 1
#define __USEPNG__ 1
#define __USEGIF__ 1

bool imageFromFile( char const *fileName,
                    image_t    &image )
{
   image.unload();
   memFile_t fIn( fileName );
   if( fIn.worked() )
   {
      bool            worked = false ;
      void const     *pixMap = 0 ;
      void const     *alpha = 0 ;
      unsigned short  width ;
      unsigned short  height ;
      char const *cData = (char const *)fIn.getData();

#ifdef __USEPNG__
      if( ( 'P' == cData[1] ) && ( 'N' == cData[2] ) && ( 'G' == cData[3] ) )
      {
         worked = imagePNG( cData, fIn.getLength(), pixMap, width, height, alpha );
      }
      else 
#endif
#ifdef __USEJPEG__
      
      if( ('\xff' == cData[0] ) && ( '\xd8' == cData[1] ) && ( '\xff' == cData[2] ) && ( '\xe0' == cData[3] ) )
      {
         worked = imageJPEG( cData, fIn.getLength(), pixMap, width, height );
      }
      else 
#endif
#ifdef __USEGIF__
      if( 0 == memcmp( fIn.getData(), "GIF8", 4 ) )
      {
         worked = imageGIF( cData, fIn.getLength(), pixMap, width, height, alpha );
      }
      else
#endif
      {
         // empty else clause
      }

      if( worked )
      {
         image.pixData_ = pixMap ;
         image.width_   = width ;
         image.height_  = height ;
         image.alpha_   = alpha ;
         return true ;
      }
   }

   return false ;

}


#ifdef STANDALONE
#include <stdio.h>
#include "fbDev.h"
#include <stdlib.h>

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      image_t image ;
      if( imageFromFile( argv[1], image ) )
      {
         unsigned x = 0 ;
         if( 3 <= argc )
            x = strtoul( argv[2], 0, 0 );

         unsigned y = 0 ;
         if( 4 <= argc )
            y = strtoul( argv[3], 0, 0 );
         getFB().render( x, y, 
                         image.width_, image.height_, 
                         (unsigned short *)image.pixData_ );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: imgFile fileName [x [y]]\n" );
   return 0 ;
}   
#endif
