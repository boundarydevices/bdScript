#ifndef __SM501ALPHA_H__
#define __SM501ALPHA_H__ "$Id: sm501alpha.h,v 1.4 2007-01-21 21:37:55 ericn Exp $"

/*
 * sm501alpha.h
 *
 * This header file declares the sm501alpha_t class,
 * to keep track of pixels on the alpha layer of the
 * SM-501 graphics controller.
 *
 * It is currently set to RGBA44 mode with a hard-coded 
 * 16-color gray-scale palette from 0 (black) to 15 (white)
 * for providing a dithered highlight and shading for 
 * odometers.
 *
 *
 * Change History : 
 *
 * $Log: sm501alpha.h,v $
 * Revision 1.4  2007-01-21 21:37:55  ericn
 * -add text output for rgba4444
 *
 * Revision 1.3  2006/08/29 01:07:41  ericn
 * -add setPixel4444 method
 *
 * Revision 1.2  2006/08/22 17:05:23  ericn
 * -rearrange declarations
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbDev.h"

class sm501alpha_t {
public:
   enum mode_t {
      rgba44 = 2,
      rgba4444 = 3
   };

   static sm501alpha_t &get( mode_t mode ); // get singleton
   

   // -------------- valid in either mode
   bool isOpen( void ) const { return 0 <= fd_ ; }
   // clear rectangle to transparent
   void clear( rectangle_t const &r );
   mode_t getMode( void ) const { return mode_ ; }
   unsigned fbRamOffset(void) const { return ramOffs_ ; }

   // -------------- only valid in 4:4 mode
   //
   // input should be a packed set of pixel values in 4:4 format
   // high nibble is transparency, low nibble is color palette entry
   //
   void set( rectangle_t const   &r,
             unsigned char const *ac44 );

   //
   // draw text (alpha) into palettized buffer
   //    alpha is reduced to 4-bits from 8
   //    color is interpreted as a palette index in mode rgba44
   //    or an rgb color (0xRRGGBB) in mode rgba4444
   //
   void drawText( unsigned char const *alpha, // 8-bit alpha
                  unsigned             srcWidth,
                  unsigned             srcHeight,
                  unsigned             destx,
                  unsigned             desty,
                  unsigned             color );


   // -------------- only valid in 4:4 mode
   void draw4444( unsigned short const *rgba4444,
                  unsigned              x,
                  unsigned              y,
                  unsigned              w,      // padded to 16-bytes
                  unsigned              h );
   void setPixel4444( unsigned x, unsigned y, unsigned long rgb );

   unsigned short *getRow( unsigned y );

private:
   sm501alpha_t( mode_t mode ); // only access through 
   mode_t     const  mode_ ;
   int               fd_ ;
   rectangle_t const pos_ ;
   unsigned char    *ram_ ;
   unsigned          ramOffs_ ;
};


#endif

