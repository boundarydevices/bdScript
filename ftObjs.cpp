/*
 * Module ftObjs.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: ftObjs.cpp,v $
 * Revision 1.5  2003-02-07 03:01:33  ericn
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

   char const *sText = dataStr ;
   for( unsigned i = 0 ; i < strLen ; i++ )
   {
      char const c = *sText++ ;

      // three things can happen here:
      //    1. we can load the char index and glyph, at which point we can ask about kerning.
      //    2. we can't load the char index, but can render directly, which means we can't kern
      //    3. everything failed
      //

      bool haveGlyph = false ;
      int  kern = 0 ;

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
                     kern = delta.x / 64 ;
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
//         fprintf( stderr, "Undefined char <0x%02x>\n", (unsigned char)(c) );
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
         char const c = *sText++ ;
   
         // three things can happen here:
         //    1. we can load the char index and glyph, at which point we can ask about kerning.
         //    2. we can't load the char index, but can render directly, which means we can't kern
         //    3. everything failed
         //
   
         bool haveGlyph = false ;
         int  kern = 0 ;
   
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
                        kern = delta.x / 64 ;
                     }
                  }
//                  else
//                     fprintf( stderr, "Error %d rendering glyph\n", error );
               }
               else
               {
//                  fprintf( stderr, "Bitmap rendered directly\n" );
                  haveGlyph = true ;
               }
            }
            else
            {
//               fprintf( stderr, "Error %d loading glyph\n", error );
            }
         }
         else
         {
            haveGlyph = ( 0 == FT_Load_Char( font.face_, c, FT_LOAD_RENDER ) );
         }
   
         if( haveGlyph )
         {
            // 
            // Whew! A lot of stuff was done to get here, but now
            // we have a bitmap (font.face_->glyph->bitmap) (actually a 
            // byte-map with the image data in 1/255ths
            //
            penX += kern ; // adjust for kerning
            
            FT_Bitmap const &bmp = font.face_->glyph->bitmap ;
            if( ( 0 < bmp.rows ) && ( 0 < bmp.width ) )
            {
               if( 256 == bmp.num_grays )
               {
                  unsigned char const *rasterLine = bmp.buffer ;
                  unsigned short nextY = penY - font.face_->glyph->bitmap_top ;
                  for( int row = 0 ; row < bmp.rows ; row++, nextY++ )
                  {
/*
                     if( nextY > height_ )
                     {
                        fprintf( stderr, "Invalid y : %d, penY = %d, height_ == %u\n", (short)nextY, penY, height_ );
                        exit(3);
                     }
*/
                     assert( nextY < height_ );
                        
                     unsigned char const *rasterCol = rasterLine ;
                     unsigned short nextX ;
                     if( ( 0 <= font.face_->glyph->bitmap_left ) 
                         ||
                         ( penX > ( 0 - font.face_->glyph->bitmap_left ) ) )
                        nextX = penX + font.face_->glyph->bitmap_left ;
                     else
                        nextX = 0 ;
                     
                     unsigned char *nextOut = data_ + (nextY*width_) + nextX ;
                     for( int col = 0 ; col < bmp.width ; col++, nextX++ )
                     {
                        if( nextX >= width_ )
                        {
                           fprintf( stderr, "Invalid x : %d, penX = %d, width_ = %u\n", (short)nextX, penX, width_ );
                           fprintf( stderr, "bitmap_left = %d\n", font.face_->glyph->bitmap_left );
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
                     rasterLine += bmp.pitch ;
                  }
               } // anti-aliased
               else if( 1 == bmp.num_grays )
               {
                  unsigned char const *rasterLine = bmp.buffer ;
                  unsigned short nextY = penY - font.face_->glyph->bitmap_top ;
                  for( int row = 0 ; row < bmp.rows ; row++, nextY++ )
                  {
                     assert( nextY < height_ );
                     unsigned char const *rasterCol = rasterLine ;
                     unsigned char mask = 0x80 ;
                     unsigned short nextX = penX + font.face_->glyph->bitmap_left ;
                     for( int col = 0 ; col < bmp.width ; col++, nextX++ )
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
                     rasterLine += bmp.pitch ;
                  }
               }
            } // non-blank
            penX  += ( ( font.face_->glyph->advance.x + 63 ) / 64 );
         }
         else
         {
//            fprintf( stderr, "Undefined char <0x%02x>\n", (unsigned char)(c) );
         }
         
         prevGlyph = glIndex ;
   
      } // walk string, calculating width and height
   }
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
         freeTypeFont_t font( fIn.getData(), fIn.getLength() );
         if( font.worked() )
         {
            char const *stringToRender = argv[3];
            unsigned    stringLen = strlen( stringToRender );

            time_t startTime ; 
            startTime = time( &startTime );
            for( unsigned i = 0 ; i < 100 ; i++ )
            {
               freeTypeString_t render( font, atoi( argv[2] ), stringToRender, stringLen );
               if( 0 == i )
                  printf( "width %u, height %u, y advance %u\n", render.getWidth(), render.getHeight(), render.getFontHeight() );
            }
            
            time_t endTime ; 
            endTime = time( &endTime );
            printf( "%u seconds\n", endTime - startTime );

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
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage : ftRender fontFile pointSize stringToRender\n" );

   return 0 ;
}

#endif
