/*
 * Module screenImage.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: screenImage.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "screenImage.h"
#include <string.h>

void screenImageRect( fbDevice_t        &fb,      // input
                      rectangle_t const &r,       // input: which portion of the screen
                      image_t           &img )    // output
{
   if( img.pixData_ )
      img.unload();

   img.pixData_ = new unsigned char [r.width_*r.height_*sizeof(unsigned short)];
   img.width_ = r.width_ ;
   img.height_ = r.height_ ;
   img.alpha_ = 0 ;

   unsigned short *const pixels = (unsigned short *)img.pixData_ ;
   for( unsigned y = 0 ; y < (unsigned)r.height_ ; y++ )
   {
      int const screenY = r.yTop_ + y ;
      if( ( 0 <= screenY ) 
          &&
          ( screenY < fb.getHeight() ) )
      {
         for( unsigned x = 0 ; x < (unsigned)r.width_ ; x++ )
         {
            int const screenX = x + r.xLeft_ ;
            if( ( 0 < screenX ) && ( screenX < fb.getWidth() ) )
            {
               pixels[(y*r.width_)+x] = fb.getPixel( screenX, screenY );
            }
            else
               pixels[(y*r.width_)+x] = 0 ;
         } // for each column
      }
      else
         memset( pixels+(y*r.width_), 0, r.width_*sizeof(pixels[0]));
   } // for each row requested
}                      


