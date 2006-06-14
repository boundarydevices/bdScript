/*
 * Module fbImage.cpp
 *
 * This module defines the frame-buffer image
 * routines as declared in fbImage.h
 *
 *
 * Change History : 
 *
 * $Log: fbImage.cpp,v $
 * Revision 1.1  2006-06-14 13:55:24  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbImage.h"
#include <string.h>


void screenImage( fbDevice_t &fb,      // input
                  image_t    &img )    // output
{
   if( img.pixData_ )
      img.unload();
      
   img.pixData_ = new unsigned char [fb.getMemSize()];
   memcpy( (void *)img.pixData_, fb.getMem(), fb.getMemSize() );
   img.width_ = fb.getWidth();
   img.height_ = fb.getHeight();
   img.alpha_ = 0 ;
}

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

void showImage( fbDevice_t    &fb,
                unsigned       x,
                unsigned       y,
                image_t const &img )
{
   fb.render( x, y, img.width_, img.height_,
              (unsigned short const *)img.pixData_, 
              (unsigned char const *)img.alpha_ );
}

