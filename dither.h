#ifndef __DITHER_H__
#define __DITHER_H__ "$Id: dither.h,v 1.3 2005-01-01 16:11:59 ericn Exp $"

/*
 * dither.h
 *
 * This header file declares the dither_t class,
 * which is used to dither a 16-bit color image
 * to black and white using the Floyd-Steinberg
 * dithering algorithm.
 *
 * Bits are packed tightly into output (rows don't
 * start on byte boundaries).
 *
 * Bit 0 is the left-most bit.
 *
 * Change History : 
 *
 * $Log: dither.h,v $
 * Revision 1.3  2005-01-01 16:11:59  ericn
 * -prevent dither copies
 *
 * Revision 1.2  2004/07/04 21:33:54  ericn
 * -added getBits() method
 *
 * Revision 1.1  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

class dither_t {
public:
   dither_t( unsigned short const rgb16[],
             unsigned width, unsigned height );
   ~dither_t( void )
   {
      if( bits_ )
         delete [] (unsigned char *)bits_ ;
   }

   inline unsigned const getWidth( void ) const { return width_ ; }
   inline unsigned const getHeight( void ) const { return height_ ; }

   inline bool isBlack( unsigned x, unsigned y ) const ;
   inline bool isWhite( unsigned x, unsigned y ) const { return !isBlack( x, y ); }
   unsigned char const *getBits(void) const { return bits_ ; }

private:
   dither_t( dither_t const & ); // no copies
   unsigned char const * const bits_ ;
   unsigned const              width_ ;
   unsigned const              height_ ;
};


bool dither_t :: isBlack( unsigned x, unsigned y ) const 
{
   if( ( x < width_ ) && ( y < height_ ) )
   {
      unsigned offs = (y*width_)+x ;
      return ( 0 == ( bits_[offs/8] & ( 1 << (offs&7) ) ) );
   }
   return true ;
}

#endif

