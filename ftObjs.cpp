/*
 * Module ftObjs.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: ftObjs.cpp,v $
 * Revision 1.8  2003-02-09 03:44:44  ericn
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
   printf( "num_faces   %ld\n", face_->num_faces );
   printf( "face_index    %ld\n", face_->face_index );

   printf( "face_flags  %ld:\n", face_->face_flags );
   for( int mask = 1, idx = 0 ; mask <= FT_FACE_FLAG_GLYPH_NAMES ; mask <<= 1, idx++ )
   {
      if( idx < numFaceFlags_ )
      {
         if( 0 != ( face_->face_flags & mask ) )
            printf( "   " );
         else
            printf( "   !" );
         printf( "%s\n", faceFlagNames_[idx] );
      }
      else
         fprintf( stderr, "How did we get here!\n" );
   } 

   printf( "style_flags  %ld\n", face_->style_flags );

   printf( "num_glyphs  %ld\n", face_->num_glyphs );

   printf( "family_name %s\n", face_->family_name );
   printf( "style_name  %s\n", face_->style_name );

   printf( "num_fixed_sizes   %d\n", face_->num_fixed_sizes );
   for( int fs = 0 ; fs < face_->num_fixed_sizes ; fs++ )
   {
      printf( "   %d/%d\n", face_->available_sizes[fs].width,
                            face_->available_sizes[fs].height );
   }

   printf( "num_charmaps   %d\n", face_->num_charmaps );


   // the following are only relevant to scalable outlines
   //    FT_BBox           bbox;

   printf( "bbox           %d/%d, %d/%d\n", face_->bbox.xMin, face_->bbox.xMax, face_->bbox.yMin, face_->bbox.yMax );
   printf( "units_per_EM   %d\n", face_->units_per_EM );
   printf( "ascender;      %d\n", face_->ascender );
   printf( "descender      %d\n", face_->descender );
   printf( "height         %d\n", face_->height );

   printf( "max_advance_width %d\n", face_->max_advance_width );
   printf( "max_advance_height %d\n", face_->max_advance_height );

   printf( "underline_position   %d\n", face_->underline_position );
   printf( "underline_thickness  %d\n", face_->underline_thickness );

   //    FT_GlyphSlot      glyph;
   //    FT_Size           size;
   //    FT_CharMap        charmap;

   for( int cm = 0; cm < face_->num_charmaps; cm++ )
   {
      FT_CharMap charmap = face_->charmaps[cm];
      printf( "charmap %d =>\n", cm );
      printf( "   encoding 0x%08x\n", charmap->encoding );
      printf( "   platform 0x%02x\n", charmap->platform_id );
      printf( "   encodeId 0x%02x\n", charmap->encoding_id );
   
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

      printf( "   -> %lu charcodes\n", numCharCodes );
   }
   
   for( int fs = 0 ; fs < face_->num_fixed_sizes ; fs++ )
   {
      FT_Bitmap_Size const *bms = face_->available_sizes + fs;
      printf( "fixed bitmap size %d =>\n", fs );
      printf( "   height %d\n", bms->height );
      printf( "   width  %d\n", bms->width );
   }
}

typedef struct glyphData_t {
   FT_Int         kern ;
   FT_Int         glyphBitmap_left ;
   FT_Int         glyphAdvanceX ;
   FT_Int         glyphBitmap_top ;
   FT_Int         glyphBitmapRows ;
   int            bmpRows ;
   int            bmpWidth ;
   int            bmpPitch ;
   unsigned char* bmpBuffer ;
   short          bmpNum_grays ;
};

freeTypeString_t :: freeTypeString_t
   ( freeTypeFont_t &font,
     unsigned        pointSize,
     char const     *dataStr,
     unsigned        strLen )
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
      FT_Set_Char_Size( font.face_, 0, pointSize*64, 80, 80 );
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
   glyphData_t  **glyphs = new glyphData_t *[strLen];

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
                           fprintf( stderr, "Invalid x : %d, penX = %d, width_ = %u\n", (short)nextX, penX, width_ );
                           fprintf( stderr, "bitmap_left = %d\n", glyph->glyphBitmap_left );
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
                        assert( nextX < width_ );
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
                     } // for each column
                     rasterLine += glyph->bmpPitch ;
                  }
               }
            } // non-blank
            penX  += ( ( glyph->glyphAdvanceX + 63 ) / 64 );
         }
      } // walk string, rendering into buffer
   }

   delete [] glyphs ;
   obstack_free( &glyphStack, 0 );
}

freeTypeString_t :: ~freeTypeString_t( void )
{
   if( data_ )
      delete [] data_ ;
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
               freeTypeString_t render( font, atoi( argv[2] ), stringToRender, stringLen );
               if( 0 == i )
                  printf( "width %u, height %u, y advance %u\n", render.getWidth(), render.getHeight(), render.getFontHeight() );
/*
            for( unsigned y = 0 ; y < render.getHeight(); y++ )
            {
               for( unsigned x = 0 ; x < render.getWidth(); x++ )
               {
                  unsigned char const alpha = render.getAlpha( x, y );
                  printf( "%02x ", alpha );
               }
               printf( "\n" );
            }
*/
            }
            else
               fprintf( stderr, "Error reading font file %s\n", argv[1] );
         }
         
         time_t endTime ; 
         endTime = time( &endTime );
         printf( "%u seconds\n", endTime - startTime );
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
            printf( "----------> information from %s\n", argv[1] );
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
