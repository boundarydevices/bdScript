/*
 * Module ftObjs.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: ftObjs.cpp,v $
 * Revision 1.11  2004-07-25 23:56:25  ericn
 * -allow rotation
 *
 * Revision 1.10  2004/07/04 21:30:35  ericn
 * -add monochrome(bitmap) support
 *
 * Revision 1.9  2003/02/10 01:16:56  ericn
 * -modified to allow truncation of text
 *
 * Revision 1.8  2003/02/09 03:44:44  ericn
 * -added filename for batch dump output
 *
 * Revision 1.7  2003/02/09 02:58:52  ericn
 * -moved font dump to ftObjs
 *
 * Revision 1.6  2003/02/07 04:41:52  ericn
 * -modified to only render once
 *
 * Revision 1.5  2003/02/07 03:01:33  ericn
 * -made freeTypeLibrary_t internal and persistent
 *
 * Revision 1.4  2003/01/20 06:49:10  ericn
 * -removed underrun and erase on negative bitmap
 *
 * Revision 1.3  2002/11/17 22:23:48  ericn
 * -rounding errors in render
 *
 * Revision 1.2  2002/11/02 18:38:48  ericn
 * -modified to initialize pixmap
 *
 * Revision 1.1  2002/11/02 04:13:07  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "ftObjs.h"
#include <assert.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>
#include "bitmap.h"

//#define DEBUGPRINT 1
#include "debugPrint.h"

#define XRES 80
#define YRES 80

// #define DEBUGPRINT 1
#include "debugPrint.h"

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
   
static freeTypeLibrary_t lib ;

freeTypeFont_t :: freeTypeFont_t
   ( void const        *data,
     unsigned long      size )
   : face_( 0 )
{
   int error = FT_New_Memory_Face( lib.library_, (FT_Byte *)data, size, 0, &face_ );
   if( 0 != error )
      face_ = 0 ;
}
   
freeTypeFont_t :: ~freeTypeFont_t( void )
{
   if( face_ )
      FT_Done_Face( face_ );
}

static char const * const faceFlagNames_[] = {
   "FT_FACE_FLAG_SCALABLE",
   "FT_FACE_FLAG_FIXED_SIZES",
   "FT_FACE_FLAG_FIXED_WIDTH",
   "FT_FACE_FLAG_SFNT",
   "FT_FACE_FLAG_HORIZONTAL",
   "FT_FACE_FLAG_VERTICAL",
   "FT_FACE_FLAG_KERNING",
   "FT_FACE_FLAG_FAST_GLYPHS",
   "FT_FACE_FLAG_MULTIPLE_MASTERS",
   "FT_FACE_FLAG_GLYPH_NAMES"
};

static unsigned const numFaceFlags_ = sizeof( faceFlagNames_ )/sizeof( faceFlagNames_[0] );

void freeTypeFont_t :: dump() const
{
   debugPrint( "num_faces   %ld\n", face_->num_faces );
   debugPrint( "face_index    %ld\n", face_->face_index );

   debugPrint( "face_flags  %ld:\n", face_->face_flags );
   for( int mask = 1, idx = 0 ; mask <= FT_FACE_FLAG_GLYPH_NAMES ; mask <<= 1, idx++ )
   {
      if( idx < numFaceFlags_ )
      {
         if( 0 != ( face_->face_flags & mask ) )
            debugPrint( "   " );
         else
            debugPrint( "   !" );
         debugPrint( "%s\n", faceFlagNames_[idx] );
      }
      else
         fprintf( stderr, "How did we get here!\n" );
   } 

   debugPrint( "style_flags  %ld\n", face_->style_flags );

   debugPrint( "num_glyphs  %ld\n", face_->num_glyphs );

   debugPrint( "family_name %s\n", face_->family_name );
   debugPrint( "style_name  %s\n", face_->style_name );

   debugPrint( "num_fixed_sizes   %d\n", face_->num_fixed_sizes );
   for( int fs = 0 ; fs < face_->num_fixed_sizes ; fs++ )
   {
      debugPrint( "   %d/%d\n", face_->available_sizes[fs].width,
                            face_->available_sizes[fs].height );
   }

   debugPrint( "num_charmaps   %d\n", face_->num_charmaps );


   // the following are only relevant to scalable outlines
   //    FT_BBox           bbox;

   debugPrint( "bbox           %d/%d, %d/%d\n", face_->bbox.xMin, face_->bbox.xMax, face_->bbox.yMin, face_->bbox.yMax );
   debugPrint( "units_per_EM   %d\n", face_->units_per_EM );
   debugPrint( "ascender;      %d\n", face_->ascender );
   debugPrint( "descender      %d\n", face_->descender );
   debugPrint( "height         %d\n", face_->height );

   debugPrint( "max_advance_width %d\n", face_->max_advance_width );
   debugPrint( "max_advance_height %d\n", face_->max_advance_height );

   debugPrint( "underline_position   %d\n", face_->underline_position );
   debugPrint( "underline_thickness  %d\n", face_->underline_thickness );

   //    FT_GlyphSlot      glyph;
   //    FT_Size           size;
   //    FT_CharMap        charmap;

   for( int cm = 0; cm < face_->num_charmaps; cm++ )
   {
      FT_CharMap charmap = face_->charmaps[cm];
      debugPrint( "charmap %d =>\n", cm );
      debugPrint( "   encoding 0x%08x\n", charmap->encoding );
      debugPrint( "   platform 0x%02x\n", charmap->platform_id );
      debugPrint( "   encodeId 0x%02x\n", charmap->encoding_id );
   
      unsigned long numCharCodes = 0 ;

#define BADCHARCODE 0xFFFFaaaa
      
      FT_ULong  startCharCode = BADCHARCODE ;
      FT_ULong  prevCharCode = BADCHARCODE ;

      FT_UInt   gindex;                                                
      FT_ULong  charcode = FT_Get_First_Char( face_, &gindex );
      while( gindex != 0 )                                            
      {                                                                
         if( charcode != prevCharCode + 1 )
         {
            if( BADCHARCODE != prevCharCode )
            {
               numCharCodes += ( prevCharCode-startCharCode ) + 1 ;
            }
            startCharCode = prevCharCode = charcode ;
         }
         else
            prevCharCode = charcode ;
         charcode = FT_Get_Next_Char( face_, charcode, &gindex );        
      }                                                                
      
      if( BADCHARCODE != prevCharCode )
         numCharCodes += ( prevCharCode-startCharCode ) + 1 ;

      debugPrint( "   -> %lu charcodes\n", numCharCodes );
   }
   
   for( int fs = 0 ; fs < face_->num_fixed_sizes ; fs++ )
   {
      FT_Bitmap_Size const *bms = face_->available_sizes + fs;
      debugPrint( "fixed bitmap size %d =>\n", fs );
      debugPrint( "   height %d\n", bms->height );
      debugPrint( "   width  %d\n", bms->width );
   }
}

typedef struct glyphData_t {
   FT_Int         kern ;
   FT_Int         glyphBitmap_left ;
   FT_Int         glyphAdvanceX ;
   FT_Int         glyphAdvanceY ;
   FT_Int         glyphBitmap_top ;
   FT_Int         glyphBitmapRows ;
   int            bmpRows ;
   int            bmpWidth ;
   int            bmpPitch ;
   int            penX ;   // in sub-pixels
   int            penY ;   // in sub-pixels
   unsigned char* bmpBuffer ;
   short          bmpNum_grays ;
};

freeTypeString_t :: freeTypeString_t
   ( freeTypeFont_t &font,
     unsigned        pointSize,
     char const     *dataStr,
     unsigned        strLen,
     unsigned        maxWidth )
   : width_( 0 ),
     height_( 0 ),
     baseline_( 0 ),
     fontHeight_( 0 ),
     data_( 0 )
{
   if( ( 0 != pointSize ) 
       && 
       ( font.face_->face_flags & FT_FACE_FLAG_SCALABLE ) )
   {
      FT_Set_Char_Size( font.face_, 0, pointSize*64, XRES, YRES );
   }
   
   FT_UInt prevGlyph = 0 ; // used for kerning
   FT_Bool useKerning = FT_HAS_KERNING( font.face_ );

   //
   // have to measure ascend and descend separately
   //
   int maxAscend  = 0 ;
   int maxDescend = 0 ;
   unsigned leftMargin = 0 ;

   struct obstack glyphStack ;
   obstack_init( &glyphStack );
   unsigned const glyphPtrSize = strLen*sizeof(glyphData_t *);
   glyphData_t  **glyphs = (glyphData_t  **)
                           obstack_alloc( &glyphStack, glyphPtrSize );
   memset( glyphs, 0, glyphPtrSize );

   char const *sText = dataStr ;
   for( unsigned i = 0 ; i < strLen ; i++ )
   {
      glyphs[i] = (glyphData_t *)obstack_alloc( &glyphStack, sizeof( glyphData_t ) );
      glyphData_t &glyph = *( glyphs[i] );
      memset( &glyph, 0, sizeof( glyph ) );

      char const c = *sText++ ;
      
      // three things can happen here:
      //    1. we can load the char index and glyph, at which point we can ask about kerning.
      //    2. we can't load the char index, but can render directly, which means we can't kern
      //    3. everything failed
      //

      bool haveGlyph = false ;

      FT_UInt glIndex = FT_Get_Char_Index( font.face_, c );
      if( 0 < glIndex )
      {
         int error = FT_Load_Glyph( font.face_, glIndex, FT_LOAD_DEFAULT );
         if( 0 == error )
         {
            if( font.face_->glyph->format != ft_glyph_format_bitmap )
            {
               error = FT_Render_Glyph( font.face_->glyph, ft_render_mode_normal );
               if( 0 == error )
               {
                  haveGlyph = true ;
                  if( useKerning && ( 0 != prevGlyph ) )
                  {
                     FT_Vector  delta;
                     FT_Get_Kerning( font.face_, prevGlyph, glIndex, ft_kerning_default, &delta );
                     glyph.kern = delta.x / 64 ;
                  }
               }
//               else
//                  fprintf( stderr, "Error %d rendering glyph\n", error );
            }
            else
            {
//               fprintf( stderr, "Bitmap rendered directly\n" );
               haveGlyph = true ;
            }
         }
         else
         {
//            fprintf( stderr, "Error %d loading glyph\n", error );
         }
      }
      else
      {
         haveGlyph = ( 0 == FT_Load_Char( font.face_, c, FT_LOAD_RENDER ) );
      }

      if( haveGlyph )
      {
         glyph.glyphBitmap_left = font.face_->glyph->bitmap_left ;
         glyph.glyphAdvanceX    = font.face_->glyph->advance.x ;
         glyph.glyphAdvanceY    = font.face_->glyph->advance.y ;
         glyph.glyphBitmap_top  = font.face_->glyph->bitmap_top ;
         glyph.bmpRows          = font.face_->glyph->bitmap.rows ;
         glyph.bmpWidth         = font.face_->glyph->bitmap.width ;
         glyph.bmpPitch         = font.face_->glyph->bitmap.pitch ;
         glyph.bmpBuffer        = (unsigned char *)obstack_copy( &glyphStack, 
                                                                 font.face_->glyph->bitmap.buffer, 
                                                                 glyph.bmpPitch * glyph.bmpRows );
         glyph.bmpNum_grays     = font.face_->glyph->bitmap.num_grays ;
         // 
         // Whew! A lot of stuff was done to get here, but now
         // we have a bitmap (face->glyph->bitmap) (actually a 
         // byte-map with the image data in 1/255ths
         //
         
         // chars like "T" have negative value for bitmap_left... aargh!
         if( 0 > font.face_->glyph->bitmap_left ) 
         {
             if( (0 - font.face_->glyph->bitmap_left ) > width_ )
             {
               leftMargin = (0 - font.face_->glyph->bitmap_left);
               width_ += leftMargin ;
             }
         }

         width_  += ( ( font.face_->glyph->advance.x + 63 ) / 64 );

         if( maxAscend < font.face_->glyph->bitmap_top )
            maxAscend = font.face_->glyph->bitmap_top ;
         int const descend = font.face_->glyph->bitmap.rows - font.face_->glyph->bitmap_top ;
         if( maxDescend < descend )
            maxDescend = descend ;
      }
      else
      {
         glyphs[i] = 0 ;
      }
      
      prevGlyph = glIndex ;

   } // walk string, calculating width and height

   baseline_ = maxAscend ;
   fontHeight_ = FT_MulFix( font.face_->height, font.face_->size->metrics.y_scale ) / 64 ;

   height_ = maxAscend + maxDescend + 1 ;

   ++width_ ;

   if( ( 0 != width_ ) && ( 0 != height_ ) )
   {
      if( width_ > maxWidth )
         width_ = maxWidth ;
      unsigned const outBytes = width_*height_ ;
      data_ = new unsigned char [ outBytes ];
      memset( data_, 0, outBytes );
      sText = dataStr ;

      unsigned penX = leftMargin ;
      unsigned penY = maxAscend ;

      for( unsigned i = 0 ; i < strLen ; i++ )
      {
         glyphData_t const *glyph = glyphs[i];
         if( glyph )
         {
            penX += glyph->kern ; // adjust for kerning
            if( ( 0 < glyph->bmpRows ) && ( 0 < glyph->bmpWidth ) )
            {
               if( 256 == glyph->bmpNum_grays )
               {
                  unsigned char const *rasterLine = glyph->bmpBuffer ;
                  unsigned short nextY = penY - glyph->glyphBitmap_top ;
                  for( int row = 0 ; row < glyph->bmpRows ; row++, nextY++ )
                  {
                     assert( nextY < height_ );
                        
                     unsigned char const *rasterCol = rasterLine ;
                     unsigned short nextX ;
                     if( ( 0 <= glyph->glyphBitmap_left ) 
                         ||
                         ( penX > ( 0 - glyph->glyphBitmap_left ) ) )
                        nextX = penX + glyph->glyphBitmap_left ;
                     else
                        nextX = 0 ;
                     
                     unsigned char *nextOut = data_ + (nextY*width_) + nextX ;
                     for( int col = 0 ; col < glyph->bmpWidth ; col++, nextX++ )
                     {
                        if( nextX >= width_ )
                        {
                           continue ;
                        }
                        
//                        assert( nextX < width_ );
                        unsigned char const rasterVal = *rasterCol++ ;
                        unsigned const oldVal = *nextOut ;
                        unsigned const sum = oldVal + rasterVal ;
                        if( 256 > sum )
                           *nextOut = (unsigned char)sum ;
                        else
                           *nextOut = '\xff' ;

                        nextOut++ ;
                     } // for each column
                     rasterLine += glyph->bmpPitch ;
                  }
               } // anti-aliased
               else if( 1 == glyph->bmpNum_grays )
               {
                  unsigned char const *rasterLine = glyph->bmpBuffer ;
                  unsigned short nextY = penY - glyph->glyphBitmap_top ;
                  for( int row = 0 ; row < glyph->bmpRows ; row++, nextY++ )
                  {
                     assert( nextY < height_ );
                     unsigned char const *rasterCol = rasterLine ;
                     unsigned char mask = 0x80 ;
                     unsigned short nextX = penX + glyph->glyphBitmap_left ;
                     for( int col = 0 ; col < glyph->bmpWidth ; col++, nextX++ )
                     {
                        if( nextX < width_ )
                        {
                           unsigned char *nextOut = data_ + (nextY*width_) + nextX ;
                           *nextOut++ = ( 0 == ( *rasterCol & mask ) )
                                        ? 0 
                                        : 255 ;
                           mask >>= 1 ;
                           if( 0 == mask )
                           {
                              mask = 0x80 ;
                              rasterCol++ ;
                           }
                        }
                        else
                           break;
                     } // for each column
                     rasterLine += glyph->bmpPitch ;
                  }
               }
            } // non-blank
            penX  += ( ( glyph->glyphAdvanceX + 63 ) / 64 );
         }
      } // walk string, rendering into buffer
   }

   obstack_free( &glyphStack, 0 );
}

freeTypeString_t :: ~freeTypeString_t( void )
{
   if( data_ )
      delete [] data_ ;
}

static FT_Matrix const deg90 = {
  (FT_Fixed)( 0 ),
  (FT_Fixed)( ((unsigned short)-1) << 16 ),
  (FT_Fixed)( ((unsigned short) 1) << 16 ),
  (FT_Fixed)( 0 )
};

static FT_Matrix const deg180 = {
  (FT_Fixed)( ((unsigned short)-1) << 16 ),
  (FT_Fixed)( 0 ),
  (FT_Fixed)( 0 ),
  (FT_Fixed)( ((unsigned short)-1) << 16 )
};

static FT_Matrix const deg270 = {
  (FT_Fixed)( 0 ),
  (FT_Fixed)( ((unsigned short) 1) << 16 ),
  (FT_Fixed)( ((unsigned short)-1) << 16 ),
  (FT_Fixed)( 0 )
};

bool freeTypeToBitmapBox( freeTypeFont_t &font,
                          unsigned        pointSize,
                          unsigned        alignment,
                          char const     *dataStr,
                          unsigned        strLen,
                          unsigned        x,
                          unsigned        y,
                          unsigned        w,
                          unsigned        h,
                          unsigned char  *bmp,
                          unsigned        bmpStride, // bytes per row
                          unsigned        bmpRows,
                          unsigned        angle ) // only zero, 90, 180, 270 supported
{
   debugPrint( "rect: x:%ld, y:%ld, w:%ld, h:%ld\n", x, y, w, h );
   debugPrint( "bbox: yMin: %ld, yMax %ld\n"
           "      xMin: %ld, xMax %ld\n", 
           font.face_->bbox.yMin,
           font.face_->bbox.yMax,
           font.face_->bbox.xMin,
           font.face_->bbox.xMax );
   //
   // Scale maximal bounding box from design (grid) units to pixels
   //
   // See http://freetype.sourceforge.net/freetype2/docs/glyphs/glyphs-2.html for details
   //
   long const pixelSize = ((long)pointSize * XRES+71) / 72 ; // should separate X/Y resolution

   debugPrint( "pixelSize: %ld\n", pixelSize );

   signed long emHeight = font.face_->bbox.yMax-font.face_->bbox.yMin ;
   signed long emWidth  = font.face_->bbox.xMax-font.face_->bbox.xMin ;

   signed long heightPixels = ( emHeight * pixelSize + font.face_->units_per_EM - 1 ) / font.face_->units_per_EM ;
   signed long widthPixels  = ( emWidth * pixelSize + font.face_->units_per_EM - 1 ) / font.face_->units_per_EM ;
   debugPrint( "bbox in pixels: %lu x %lu\n", widthPixels, heightPixels );
   signed long ascendPixels = ( font.face_->ascender * pixelSize 
                                + font.face_->units_per_EM - 1 ) 
                              / font.face_->units_per_EM ;
   debugPrint( "ascender in pixels: %lu\n", ascendPixels );
   signed long maxBearingX  = ( ( (0L-font.face_->bbox.xMin) * pixelSize )
                                - (font.face_->units_per_EM - 1) )
                              / font.face_->units_per_EM ;
   debugPrint( "maxBearingX = %ld\n", maxBearingX );

   FT_Vector pen ;

   if( ( 0 != pointSize ) 
       && 
       ( font.face_->face_flags & FT_FACE_FLAG_SCALABLE ) )
   {
      FT_Set_Char_Size( font.face_, 0, pointSize*64, XRES, YRES );
   }

//   FT_Matrix _m ;
   FT_Matrix const *matrix = 0 ;
   if( 0 == angle )
   {
      debugPrint( "0 degree rotation\n" );
      matrix = 0 ;
      pen.x = x + maxBearingX ;
      pen.y = y + (((signed long)h-heightPixels)/2) + ascendPixels ;
   }
   else if( 90 == angle )
   {
      debugPrint( "90 degree rotation\n" );
      matrix = &deg90 ;
      pen.y  = y + h - maxBearingX ;
      pen.x  = x + (((signed long)w-heightPixels)/2) + ascendPixels ;
   }
   else if( 180 == angle )
   {
      debugPrint( "180 degree rotation\n" );
      matrix = &deg180 ;
      pen.x  = x + w - maxBearingX ;
      pen.y  = (signed long)y + h - (((signed long)h-heightPixels)/2) - ascendPixels ;
   }
   else if( 270 == angle )
   {
      debugPrint( "270 degree rotation\n" );
      matrix = &deg270 ;
      pen.y  = y + maxBearingX ;
      pen.x = (signed long)x + w - (((signed long)w-heightPixels)/2) - ascendPixels ;
   }
   else
   {
      debugPrint( "%ld degree rotation not supported\n", angle );
      return false ;
/*
      double _angle = (angle/360.0) * 3.14159 * 2 ;
      _m.xx = (FT_Fixed)( cos( _angle ) * 0x10000L );
      _m.xy = (FT_Fixed)(-sin( _angle ) * 0x10000L );
      _m.yx = (FT_Fixed)( sin( _angle ) * 0x10000L );
      _m.yy = (FT_Fixed)( cos( _angle ) * 0x10000L );
      matrix = &_m ;
*/      
   }

   debugPrint( "penPix: %ld/%ld\n", pen.x, pen.y );
   long const yBase = pen.y ;
   pen.y = 0 ;
   pen.x *= 64 ;
   pen.y *= 64 ;

   FT_Vector const startPen = pen ;

   FT_GlyphSlot slot = font.face_->glyph ;

   struct obstack glyphStack ;
   obstack_init( &glyphStack );
   unsigned const glyphPtrSize = strLen*sizeof(FT_GlyphSlot);
   FT_GlyphSlot  *glyphs = (FT_GlyphSlot *)
                           obstack_alloc( &glyphStack, glyphPtrSize );
   memset( glyphs, 0, glyphPtrSize );
   
   signed long minY = 0x7FFFFFFF ;
   signed long maxY = 0x80000000 ;
   signed long minX = 0x7FFFFFFF ;
   signed long maxX = 0x80000000 ;

   /* set transformation */
   char const *sText = dataStr ;
   for( unsigned i = 0 ; i < strLen ; i++ )
   {
      FT_Set_Transform( font.face_, (FT_Matrix *)matrix, &pen );

      /* load glyph image into the slot (erase previous one) */
      char const c = *sText++ ;
      FT_Error error = FT_Load_Char( font.face_, c, FT_LOAD_RENDER|FT_LOAD_TARGET_MONO );
      if ( error )
         continue;                 /* ignore errors */
      
      glyphs[i] = (FT_GlyphSlot)obstack_copy( &glyphStack, 
                                              slot,
                                              sizeof(*slot) );
      glyphs[i]->bitmap.buffer = (unsigned char *)
                                 obstack_copy( &glyphStack, 
                                               slot->bitmap.buffer, 
                                               slot->bitmap.rows * slot->bitmap.pitch );
      if( ( 0 < slot->bitmap.rows ) && ( 0 < slot->bitmap.width ) )
      {
         int nextY = yBase - slot->bitmap_top ;
         if( nextY < minY )
            minY = nextY ;
         if( nextY > maxY )
            maxY = nextY ;
         nextY += slot->bitmap.rows ;
         if( nextY > maxY )
            maxY = nextY ;
         if( nextY < minY )
            minY = nextY ;
         
         int penX  = slot->bitmap_left ;  
         if( penX < minX )
            minX = penX ;
         if( penX > maxX )
            maxX = penX ;
         penX += slot->bitmap.width ;
         if( penX < minX )
            minX = penX ;
         if( penX > maxX )
            maxX = penX ;
      }

      /* increment pen position */
      pen.x += slot->advance.x;
      pen.y += slot->advance.y;
   } // walk string, calculating width and height

   signed long wActual = maxX-minX ;
   signed long hActual = maxY-minY ;

   debugPrint( "x: %ld..%ld, y: %ld..%ld\n", minX, maxX, minY, maxY );
   debugPrint( "wActual: %lu, hActual: %lu\n", wActual, hActual );

   signed long xOffs ;
   signed long yOffs ;
   if( 0 == angle )
   {
      if( alignment & ftRight )
      {
         xOffs = (signed long)w-(signed long)wActual ;
      }
      else if( alignment & ftCenterHorizontal )
      {
         xOffs = ((signed long)w-(signed long)wActual)/2 ;
         debugPrint( "xOffs = %ld\n", xOffs );
      }
      else
      {
         xOffs = 0 ;
      }
      xOffs -= maxBearingX ;
      yOffs = 0 ;
   }
   else if( 90 == angle )
   {
      if( alignment & ftRight )
      {
         yOffs = (signed long)hActual-(signed long)h ;
      }
      else if( alignment & ftCenterHorizontal )
      {
         yOffs = ((signed long)hActual-(signed long)h)/2 ;
      }
      else
      {
         yOffs = 0 ;
      }
      yOffs += maxBearingX ;
      xOffs = 0 ;
   }
   else if( 180 == angle )
   {
      if( alignment & ftRight )
      {
         xOffs = (signed long)wActual-(signed long)w ;
      }
      else if( alignment & ftCenterHorizontal )
      {
         xOffs = ((signed long)wActual-(signed long)w)/2 ;
      }
      else
      {
         xOffs = 0 ;
      }
      xOffs += maxBearingX ;
      yOffs = 0 ;
   }
   else if( 270 == angle )
   {
      if( alignment & ftRight )
      {
         yOffs = (signed long)h-(signed long)hActual ;
      }
      else if( alignment & ftCenterHorizontal )
      {
         yOffs = ((signed long)h-(signed long)hActual)/2 ;
      }
      else
      {
         yOffs = 0 ;
      }
      yOffs -= maxBearingX ;
      xOffs = 0 ;
   }

debugPrint( "xOffs: %ld, yOffs: %ld\n", xOffs, yOffs );

   sText = dataStr ;
   unsigned const bottomY = y + h ;
   unsigned const right = x + w ;
   for( unsigned i = 0 ; i < strLen ; i++ )
   {
      char const c = *sText++ ;
      slot = glyphs[i];
      if( slot )
      {
         if( ( 0 < slot->bitmap.rows ) && ( 0 < slot->bitmap.width ) )
         {
            long nextY = yBase - slot->bitmap_top + yOffs ;
            long penX  = slot->bitmap_left + xOffs ; 
   debugPrint( "char %c, x:%ld, y:%ld, top %ld, left %ld\n", c, penX, nextY, slot->bitmap_top, slot->bitmap_left );
            // convert to pixel positions
            // convert Y in direction as well
            unsigned char const *nextIn = slot->bitmap.buffer ;
            long inPix = slot->bitmap.width ;
            unsigned inOffs = 0 ;
            if( penX < x )
            {
               long diff = (long)x - penX ;
               penX += diff ;
               nextIn += (diff/8);
               inOffs  = diff&7 ;
               inPix  -= diff ;
            }
            if( penX + inPix > right )
            {
               inPix -= penX + inPix - right - 1 ;
            }
            unsigned char *nextOut = bmp + ( nextY * bmpStride );
            for( unsigned row = 0 ; ( row < slot->bitmap.rows ) ; row++, nextY++ )
            {
               if( ( nextY >= y ) && ( nextY < bottomY ) 
                   &&
                   ( 0 < inPix ) )
               {
                  bitmap_t::bltFrom( nextOut, penX, nextIn, inPix, inOffs );
               } // row is in range
               nextIn += slot->bitmap.pitch ;
               nextOut += bmpStride ;
            }
         } // non-blank
      }
      else
         debugPrint( "No glyph for char %c\n", c );
   } // walk string again, placing bits

   obstack_free( &glyphStack, 0 );

   return true ;
}


#ifdef __MODULETEST__

#include "memFile.h"
#include <time.h>

int main( int argc, char const * const argv[] )
{
   if( 4 == argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         char const *stringToRender = argv[3];
         unsigned    stringLen = strlen( stringToRender );

         time_t startTime ; 
         startTime = time( &startTime );
         for( unsigned i = 0 ; i < 100 ; i++ )
         {
            freeTypeFont_t font( fIn.getData(), fIn.getLength() );
            if( font.worked() )
            {
               freeTypeString_t render( font, atoi( argv[2] ), stringToRender, stringLen, 0xFFFFFFFF );
               if( 0 == i )
                  debugPrint( "width %ld, height %ld, y advance %ld\n", render.getWidth(), render.getHeight(), render.getFontHeight() );
/*
            for( unsigned y = 0 ; y < render.getHeight(); y++ )
            {
               for( unsigned x = 0 ; x < render.getWidth(); x++ )
               {
                  unsigned char const alpha = render.getAlpha( x, y );
                  debugPrint( "%02x ", alpha );
               }
               debugPrint( "\n" );
            }
*/
            }
            else
               fprintf( stderr, "Error reading font file %s\n", argv[1] );
         }
         
         time_t endTime ; 
         endTime = time( &endTime );
         debugPrint( "%u seconds\n", endTime - startTime );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage : ftRender fontFile pointSize stringToRender\n" );

   return 0 ;
}

#else
#ifdef __DUMP__

#include "memFile.h"
#include <time.h>

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         freeTypeFont_t font( fIn.getData(), fIn.getLength() );
         if( font.worked() )
         {
            debugPrint( "----------> information from %s\n", argv[1] );
            font.dump();
         }
         else
            fprintf( stderr, "Error reading font file %s\n", argv[1] );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage : ftDump fontFile\n" );

   return 0 ;
}

#endif
#endif
