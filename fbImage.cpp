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
 * Revision 1.5  2007-08-23 00:26:47  ericn
 * -add stand-alone program
 *
 * Revision 1.4  2002/12/15 05:41:41  ericn
 * -Added scaleHorizontal() method
 *
 * Revision 1.3  2006/11/06 10:33:54  ericn
 * -allow construction from fbMem
 *
 * Revision 1.2  2006/08/16 02:38:07  ericn
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
#include <assert.h>
#include <stdio.h>

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

fbImage_t::fbImage_t( fbPtr_t &mem, unsigned w, unsigned h ) // from fb mem
   : mode_(rgb565)
   , w_( w )
   , h_( h )
   , stride_( ((w+7)/8)*8 )
   , ptr_(mem)
{
}

fbImage_t::~fbImage_t(void)
{
}

fbImage_t *fbImage_t::scaleHorizontal( unsigned width ) const 
{
   fbImage_t *newOne = new fbImage_t ;
   newOne->mode_   = mode_ ;
   newOne->w_      = width ;
   newOne->stride_ = ((width+7)/8)*8 ;
   newOne->h_      = h_ ;
   newOne->ptr_    = fbPtr_t( h_ * newOne->stride_ *sizeof(unsigned short) );

   assert( mode_ == rgba4444 );

   unsigned const inMult = (w_*256)/width ;

   unsigned short const *inPix = (unsigned short const *)ptr_.getPtr();
   unsigned short *outPix = (unsigned short *)newOne->ptr_.getPtr();
   for( unsigned y = 0 ; y < h_ ; y++ ){
      for( unsigned x = 0 ; x < width ; x++ ){
         unsigned const inPix256 = x*inMult ;
         unsigned const rightFrac = inPix256 & 255 ;
         unsigned const inPix0 = (inPix256/256);
         if( rightFrac ){
            unsigned leftFrac = (256-rightFrac);
            unsigned short left = inPix[inPix0];
            unsigned short right = inPix[inPix0+1];
            unsigned char alpha = (((left>>12)*leftFrac)+((right>>12)*rightFrac))/256 ;
            unsigned char red = ((((left>>8)&0x0F)*leftFrac)+(((right>>8)&0x0F)*rightFrac))/256 ;
            unsigned char green = ((((left>>4)&0x0F)*leftFrac)+(((right>>4)&0x0F)*rightFrac))/256 ;
            unsigned char blue = (((left&0x0F)*leftFrac)+((right&0x0F)*rightFrac))/256 ;
            assert( alpha < 16 );
            assert( red < 16 );
            assert( green < 16 );
            assert( blue < 16 );
            outPix[x] = (alpha<<12)|(red<<8)|(green<<4)|blue ;
         }
         else
            outPix[x] = inPix[inPix0];
      }

      outPix += newOne->stride_ ;
      inPix  += stride_ ;
   }
   
   return newOne ;
}

#ifdef STANDALONE
#include <stdio.h>
#include "fbDev.h"
#include "imgFile.h"
#include "sm501alpha.h"
#include <stdlib.h>

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      image_t image ;
      if( imageFromFile( argv[1], image ) )
      {
         fbImage_t fbi( image, fbImage_t::rgba4444 );
         sm501alpha_t &alpha = sm501alpha_t::get(sm501alpha_t::rgba4444);
         alpha.draw4444( (unsigned short const *)fbi.pixels(),
                         0, 0, fbi.width(), fbi.height() );

         char inBuf[80];
         gets(inBuf);
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: imgFile fileName [x [y]]\n" );
   return 0 ;
}   
#endif
