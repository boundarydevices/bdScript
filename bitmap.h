#ifndef __BITMAP_H__
#define __BITMAP_H__ "$Id: bitmap.h,v 1.1 2004-07-04 21:35:14 ericn Exp $"

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
 * Revision 1.1  2004-07-04 21:35:14  ericn
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

   inline static unsigned bytesPerRow( unsigned bitWidth ){ return ((bitWidth+31)/32)*sizeof(long); }

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
   // input bits are copied from high bit
   //
   static void bltFrom( unsigned char       *bitStart,
                        unsigned             startOffs,
                        unsigned char const *src,
                        unsigned             count );

private:
   unsigned char *const bits_ ;
   unsigned       const bitWidth_ ;
   unsigned       const bitHeight_ ;
   unsigned       const bytesPerRow_ ;
};


#endif

