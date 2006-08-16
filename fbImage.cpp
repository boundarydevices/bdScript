/*
 * Module fbImage.cpp
 *
 * This module defines the methods of the fbImage_t
 * class as declared in fbImage.h
 *
 *
 * Change History : 
 *
 * $Log: fbImage.cpp,v $
 * Revision 1.2  2006-08-16 02:38:07  ericn
 * -update to use fbPtr_t, allow 4444 support
 *
 * Revision 1.1  2006/06/14 13:55:24  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbImage.h"
#include <string.h>
#include "fbDev.h"
#include "img4444.h"

fbImage_t::fbImage_t()
   : mode_(rgb565)
   , w_( 0 )
   , h_( 0 )
   , stride_( 0 )
   , ptr_()
{
}

fbImage_t::fbImage_t( 
   image_t const &image,
   mode_t         mode )
   : mode_(mode)
   , w_( image.width_ )
   , h_( image.height_ )
   , stride_( ((image.width_+7)/8)*8 )
   , ptr_( h_*stride_*sizeof(unsigned short) )
{
   unsigned short const *inPix = (unsigned short const *)image.pixData_ ;
   unsigned short *outPix = (unsigned short *)ptr_.getPtr();
   if( outPix && inPix )
   {
      if( rgb565 == mode ){
         for( unsigned y = 0 ; y < h_ ; y++ ){
            memcpy( outPix, inPix, w_*sizeof(*outPix));
            outPix += stride_ ;
            inPix  += w_ ;
         }
      }
      else {
         imgTo4444( inPix, w_, h_, 
                    (unsigned char *)image.alpha_,
                    outPix, stride_ );
      }
   }
}

fbImage_t::fbImage_t( 
   unsigned x,
   unsigned y,
   unsigned w,
   unsigned h
)
   : mode_(rgb565)
   , w_( w )
   , h_( h )
   , stride_( ((w+7)/8)*8 )
   , ptr_( h_*stride_*sizeof(unsigned short) )
{
   fbDevice_t &fb = getFB();
   unsigned short const *inPix = ((unsigned short const *)fb.getMem()) 
                                 + y*fb.getWidth()
                                 + x ;
   unsigned short *outPix = (unsigned short *)ptr_.getPtr();
   if( outPix )
   {
      for( y = 0 ; y < h_ ; y++ ){
         memcpy( outPix, inPix, w_*sizeof(*outPix));
         outPix += stride_ ;
         inPix  += fb.getWidth();
      }
   }
}

fbImage_t::~fbImage_t(void)
{
}

