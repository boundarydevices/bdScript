#ifndef __FTOBJS_H__
#define __FTOBJS_H__ "$Id: ftObjs.h,v 1.1 2002-11-02 04:13:07 ericn Exp $"

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
 * Revision 1.1  2002-11-02 04:13:07  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <ft2build.h>
#include FT_FREETYPE_H

class freeTypeLibrary_t {
public:
   freeTypeLibrary_t( void )
   {
      FT_Init_FreeType( &library_ );
   }
   
   ~freeTypeLibrary_t( void )
   {
      FT_Done_FreeType( library_ );
   }

   FT_Library library_ ;

};
   
class freeTypeFont_t {
public:
   freeTypeFont_t( freeTypeLibrary_t &lib,
                   void const        *data,
                   unsigned long      size )
      : lib_( lib ),
        face_( 0 )
   {
      int error = FT_New_Memory_Face( lib_.library_, (FT_Byte *)data, size, 0, &face_ );
      if( 0 != error )
         face_ = 0 ;
   }
   
   ~freeTypeFont_t( void )
   {
      if( face_ )
         FT_Done_Face( face_ );
   }

   bool worked( void ) const { return 0 != face_ ; }

   freeTypeLibrary_t &lib_ ;
   FT_Face            face_ ;

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

   unsigned        width_ ;
   unsigned        height_ ;
   int             baseline_ ;
   unsigned        fontHeight_ ;
   unsigned char  *data_ ;
};

#endif

