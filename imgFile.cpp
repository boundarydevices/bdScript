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
 * Revision 1.1  2003-10-18 19:16:16  ericn
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

#define __USEJPEG__ 1
// #define __USEPNG__ 1
// #define __USEGIF__ 1

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

