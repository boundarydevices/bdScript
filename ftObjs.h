#ifndef __FTOBJS_H__
#define __FTOBJS_H__ "$Id: ftObjs.h,v 1.4 2003-02-09 02:58:52 ericn Exp $"

/*
 * ftObjs.h
 *
 * This header file declares some FreeType classes for
 * convenience.
 *
 *    freeTypeLibrary_t    - used to open/close the freetype library
 *    freeTypeFont_t       - used to represent an in-memory font file
 *    freeTypeString_t     - used to render a string in a particular font
 *
 * The primary interface for rendering is the freeTypeString_t 
 * class, which is used to generate bytemap of the alpha of the
 * string (fraction of coverage in 1/255ths).
 *
 * Change History : 
 *
 * $Log: ftObjs.h,v $
 * Revision 1.4  2003-02-09 02:58:52  ericn
 * -moved font dump to ftObjs
 *
 * Revision 1.3  2003/02/07 03:01:33  ericn
 * -made freeTypeLibrary_t internal and persistent
 *
 * Revision 1.2  2002/11/02 18:38:28  ericn
 * -added method getDataLength()
 *
 * Revision 1.1  2002/11/02 04:13:07  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <ft2build.h>
#include FT_FREETYPE_H

class freeTypeFont_t {
public:
   freeTypeFont_t( void const        *data,
                   unsigned long      size );
   ~freeTypeFont_t( void );

   bool worked( void ) const { return 0 != face_ ; }

   void dump( void ) const ;

   FT_Face face_ ;
};


class freeTypeString_t {
public:
   freeTypeString_t( freeTypeFont_t &font,
                     unsigned        pointSize,
                     char const     *dataStr,
                     unsigned        strLen );
   ~freeTypeString_t( void );

   // bitmap dimensions
   unsigned getWidth( void ) const { return width_ ; }
   unsigned getHeight( void ) const { return height_ ; }
   
   // distance from top row of bitmap to baseline (may be negative)
   int getBaseline( void ) const { return baseline_ ; }
   
   // font height (for vertical spacing)
   unsigned getFontHeight( void ) const { return fontHeight_ ; }

   unsigned char getAlpha( unsigned x, unsigned y ) const 
      { return data_[(y*width_)+x]; }
   unsigned char const *getRow( unsigned y ) const { return data_+(y*width_); }

   unsigned getDataLength( void ) const { return width_*height_ ; }

   unsigned        width_ ;
   unsigned        height_ ;
   int             baseline_ ;
   unsigned        fontHeight_ ;
   unsigned char  *data_ ;
};

#endif

