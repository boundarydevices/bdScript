#ifndef __BITMAP_H__
#define __BITMAP_H__ "$Id: bitmap.h,v 1.3 2004-09-25 21:49:07 ericn Exp $"

/*
 * bitmap.h
 *
 * This header file declares the bitmap_t class for 
 * handling a bitmap with the following attributes:
 *
 *    Rows are longword aligned
 *    Bit zero in a row is the high bit of the first byte...
 *
 * Note that this class does not own the bit memory, 
 * since that is presumed to be a part of a larger object.
 *
 * Change History : 
 *
 * $Log: bitmap.h,v $
 * Revision 1.3  2004-09-25 21:49:07  ericn
 * -added getter methods
 *
 * Revision 1.2  2004/07/25 22:34:45  ericn
 * -added source offset param to bltFrom
 *
 * Revision 1.1  2004/07/04 21:35:14  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


class bitmap_t {
public:
   bitmap_t( unsigned char *bits,
             unsigned       bitWidth,
             unsigned       bitHeight );

   // draw a rectangle (inclusive of corners)
   void rect( unsigned x, unsigned y,
              unsigned w, unsigned h,
              bool setNotClear );
   void line( unsigned x1, unsigned y1,
              unsigned x2, unsigned y2,
              unsigned penWidth,
              bool setNotClear );

   inline unsigned getWidth( void ) const { return bitWidth_ ; }
   inline unsigned getHeight( void ) const { return bitHeight_ ; }
   inline static unsigned bytesPerRow( unsigned bitWidth ){ return ((bitWidth+31)/32)*sizeof(long); }
   inline unsigned bytesPerRow( void ) const { return bitmap_t::bytesPerRow( bitWidth_ ); }

   //
   // Routines to set or clear a range of adjacent bits.
   // Note that these routines do no range checking!
   // Use with care.
   //
   static void setBits( unsigned char *bitStart,
                        unsigned       startOffs,
                        unsigned       count );
   static void clearBits( unsigned char *bitStart,
                          unsigned       startOffs,
                          unsigned       count );
   //
   // copy bits to specified offset
   // bits are numbered from high bit
   //
   static void bltFrom( unsigned char       *bitStart,
                        unsigned             startOffs,
                        unsigned char const *src,
                        unsigned             count,
                        unsigned             srcOffs = 0 );

   inline unsigned char const *getMem( void ) const { return bits_ ; }

private:
   unsigned char *const bits_ ;
   unsigned       const bitWidth_ ;
   unsigned       const bitHeight_ ;
   unsigned       const bytesPerRow_ ;
};


#endif

