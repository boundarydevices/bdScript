/*
 * Module sm501alpha.cpp
 *
 * This module defines the methods of the sm501alpha_t class,
 * as declared in sm501alpha.h
 *
 *
 *
 * Change History : 
 *
 * $Log: sm501alpha.cpp,v $
 * Revision 1.3  2007-01-21 21:37:58  ericn
 * -add text output for rgba4444
 *
 * Revision 1.2  2006/08/29 01:07:43  ericn
 * -add setPixel4444 method
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "sm501alpha.h"
#include <sys/ioctl.h>
#include <linux/sm501alpha.h>
#include <linux/sm501-int.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include "fbDev.h"
#include <string.h>
#include <assert.h>

static sm501alpha_t *inst = 0 ;

sm501alpha_t &sm501alpha_t::get( mode_t mode ){
   if( 0 == inst )
      inst = new sm501alpha_t(mode);

   assert( mode == inst->getMode() );

   return *inst ;
}
   
void sm501alpha_t::set
   ( rectangle_t const &r,
     unsigned char const *ac44 )
{
   unsigned const inStride  = r.width_ ;
   unsigned const outStride = pos_.width_ ;

   unsigned char *nextOut = ram_ 
                          + (r.yTop_-pos_.yTop_)*outStride 
                          + (r.xLeft_-pos_.xLeft_);
   for( unsigned y = 0 ; y < r.height_ ; y++ )
   {
      memcpy( nextOut, ac44, inStride );
      ac44 += inStride ;
      nextOut += outStride ;
   }
}

void sm501alpha_t::clear
   ( rectangle_t const &r )
{
   unsigned const bytesPerPixel = 1 + (rgba4444==mode_);
   unsigned const inStride  = r.width_ * bytesPerPixel ;
   unsigned const outStride = pos_.width_ * bytesPerPixel ;

   unsigned char *nextOut = ram_ 
                            + (r.yTop_-pos_.yTop_)*outStride 
                            + ((r.xLeft_-pos_.xLeft_)* bytesPerPixel);
   for( unsigned y = 0 ; y < r.height_ ; y++ )
   {
      memset( nextOut, 0, inStride );
      nextOut += outStride ;
   }
}

sm501alpha_t::sm501alpha_t( mode_t mode )
   : mode_( mode )
   , fd_( open( "/dev/alpha", O_RDWR ) )
   , pos_( makeRect(0,0,getFB().getWidth(), getFB().getHeight()) )
   , ram_( 0 )
   , ramOffs_( 0 )
{
   if( isOpen() ){
      struct sm501_alphaPlane_t plane ;
      fbDevice_t &fb = getFB();
   
      plane.xLeft_  = pos_.xLeft_ ;
      plane.yTop_   = pos_.yTop_ ;
      plane.width_  = pos_.width_ ;
      plane.height_ = pos_.height_ ;
      plane.mode_   = mode ;
      plane.planeOffset_ = 0 ;

      unsigned short const adder = (2<<11)|(4<<5)|2 ;
      unsigned short *outWords = (unsigned short *)plane.palette_ ;
      unsigned short palVal = 0 ;
      
//      printf( "color palette:\n" );
      for( unsigned i = 0 ; i < 16 ; i++ ){
//         printf( "   %04x\n", palVal );
         *outWords++ = palVal ;
         palVal += adder ;
      }
      
      outWords[-1] = 0xFFFF ; // make it truly white

      int rval = ioctl( fd_, SM501ALPHA_SETPLANE, &plane );
      if( 0 == rval ){
         unsigned char *const fbStart = (unsigned char *)fb.getMem();
         ram_ = fbStart + plane.planeOffset_ ;
         ramOffs_ = plane.planeOffset_ ;
         return ;
      }
      else
         perror( "SM501ALPHA_SETPLANE" );
      close( fd_ );
      fd_ = 0 ;
   }
}

void sm501alpha_t::drawText( 
   unsigned char const *alpha, // 8-bit alpha
   unsigned             srcWidth,
   unsigned             srcHeight,
   unsigned             destx,
   unsigned             desty,
   unsigned             color )
{
   if( isOpen() ){
      if( rgba44 == mode_ ){
         color &= 0x0F ;
         unsigned const outStride = pos_.width_ ;
         unsigned char *nextOut = ram_ 
                                  + (desty-pos_.yTop_)*outStride 
                                  + (destx-pos_.xLeft_);
         for( unsigned y = 0 ; y < srcHeight ; y++ )
         {
            for( unsigned x = 0 ; x < srcWidth ; x++ ){
               unsigned char a = *alpha++ ;
               if( a ){
                  a >>= 4 ;
                  unsigned char inVal = nextOut[x];
                  if( inVal & 0xF0 ){
                     // poor-man's blend
                     unsigned char inAlpha = inVal >> 4 ;
                     unsigned char outAlpha = (inAlpha + a)/2 ;
                     unsigned char inColor = inVal & 0x0F ;
                     unsigned char outColor = (color + inColor)/2 ;
                     nextOut[x] = (outAlpha<<4)|outColor ;
                  }
                  else
                     nextOut[x] = (a<<4)|color ;
               } // at least semi-opaque
            }
            nextOut += outStride ;
         }
      }
      else {
         unsigned const outStride = 2*pos_.width_ ;
         unsigned short *nextOut = (unsigned short *)
                                   ( ram_ 
                                     + (desty-pos_.yTop_)*outStride 
                                     + (destx-pos_.xLeft_*sizeof(unsigned short)) );
         unsigned short const rgb444 = (color & 0xF00000)>>12
                                     | (color & 0x00F000)>>8
                                     | (color & 0x0000F0)>>4 ;
         for( unsigned y = 0 ; y < srcHeight ; y++ )
         {
            for( unsigned x = 0 ; x < srcWidth ; x++ ){
               unsigned char a = *alpha++ ;
               a >>= 4 ;
               nextOut[x] = (((unsigned)a)<<12) | rgb444 ;
            }
            nextOut += pos_.width_ ;
         }
      }
   }
}

void sm501alpha_t::draw4444( 
   unsigned short const *in,
   unsigned              x,
   unsigned              y,
   unsigned              w,      // padded to 16-bytes
   unsigned              h )
{
   unsigned const stride = ((w+7)/8)*8 ;
   x -= pos_.xLeft_ ;
   y -= pos_.yTop_ ;
   
   if( w > pos_.width_ )
      w = pos_.width_ ;
   
   if( h > pos_.height_ )
      h = pos_.height_ ;

   unsigned short *outWords = ((unsigned short *)ram_ )
                            + y*pos_.width_ + x ;
   for( y = 0 ; y < h ; y++ ){
      for( x = 0 ; x < w ; x++ ){
         outWords[x] = in[x];
      }
      outWords += pos_.width_ ;
      in += stride ;
   }
}
   
void sm501alpha_t::setPixel4444( unsigned x, unsigned y, unsigned long rgb )
{
   if( ( pos_.xLeft_ <= x )
       &&
       ( pos_.xLeft_ + pos_.width_ > x ) ){
      if( ( pos_.yTop_ <= y )
          &&
          ( pos_.yTop_ + pos_.height_ > y ) ){
         unsigned xOffs = x-pos_.xLeft_ ;
         unsigned yOffs = y-pos_.yTop_ ;
         unsigned short rgba = 0xF000
                             + ((rgb>>(20-8))&0x0F00)    // top 4 of red in bits 8..11
                             + ((rgb>>(12-4))&0x00F0)    // top 4 of green in bits 4..7
                             + ((rgb>>4)&0x0F);          // top 4 of blue in 0..3

         unsigned short *outWords = ((unsigned short *)ram_ )
                                  + yOffs*pos_.width_ + xOffs ;
         *outWords = rgba ;
      }
   }
}

unsigned short *sm501alpha_t::getRow( unsigned y )
{
   unsigned bytesPerPixel = 1 + ( rgba4444 == mode_ );
   return (unsigned short *)
          (ram_+ (pos_.width_*bytesPerPixel));
}

#ifdef MODULETEST


int main( int argc, char const * const argv[] )
{
   sm501alpha_t::mode_t mode = ( (1 < argc) && (0 == strcmp("16",argv[1])) )
                             ? sm501alpha_t::rgba4444
                             : sm501alpha_t::rgba44 ;

   sm501alpha_t &alpha = sm501alpha_t::get(mode);
   if( alpha.isOpen() ){
      char inBuf[80];
      fgets(inBuf, sizeof(inBuf),stdin);
   }

   return 0 ;
}

#endif
