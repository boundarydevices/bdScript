/*
 * Module jsText.cpp
 *
 * This module defines the initialization routine and 
 * JavaScript interpreter support routines for rendering
 * text to the frame buffer device as described in 
 * jsText.h
 *
 * Change History : 
 *
 * $Log: jsText.cpp,v $
 * Revision 1.2  2002-10-20 16:31:06  ericn
 * -added bg color support
 *
 * Revision 1.1  2002/10/18 01:18:25  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsText.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include "fbDev.h"
#include <ctype.h>

/*
static FT_Library library ;

static void initFreeType( void )
{
   static bool _init = false ;
   if( !_init )
   {
      int error = FT_Init_FreeType( &library );
      if( 0 == error )
      {
         _init = true ;
//         printf( "Initialized FreeType!\n" );
      }
      else
      {
         fprintf( stderr, "Error %d initializing freeType library\n", error );
         exit( 3 );
      }
   }
}
*/

static void render( fbDevice_t      &fb,
                    FT_Bitmap const &bmp,
                    int              startX,
                    int              startY,
                    unsigned long    fgColor,         // 0xRRGGBB
                    unsigned long    bgColor )        // 0xRRGGBB
{
   unsigned char fgRed   = ( fgColor >> 16 ) & 0xFF ;
   unsigned char fgGreen = ( fgColor >> 8 )  & 0xFF ;
   unsigned char fgBlue  = ( fgColor ) & 0xFF ;

   unsigned char bgRed   = ( bgColor >> 16 ) & 0xFF ;
   unsigned char bgGreen = ( bgColor >> 8 )  & 0xFF ;
   unsigned char bgBlue  = ( bgColor ) & 0xFF ;


/*
printf( "colors == fg %u/%u/%u, bg %u/%u/%u\n", 
        fgRed, fgGreen, fgBlue,
        bgRed, bgGreen, bgBlue );
*/

   if( ( 0 < bmp.rows ) && ( 0 < bmp.width ) )
   {
      if( 256 == bmp.num_grays )
      {
         unsigned char const *rasterLine = bmp.buffer ;
         for( int row = 0 ; ( row < bmp.rows ) && ( startY < fb.getHeight() ); row++, startY++ )
         {
            if( 0 <= startY )
            {
               unsigned char const *rasterCol = rasterLine ;
               int penX = startX ;
               for( int col = 0 ; ( col < bmp.width ) && ( penX < fb.getWidth() ); col++, penX++ )
               {
                  if( 0 <= penX )
                  {
                     unsigned const charCov = *rasterCol++ ;
                     if( 0 < charCov )
                     {
                        // convert to 1..256 to use shifts instead of div for mixing
                        unsigned const alias = charCov + 1 ;
                        unsigned const notAlias = 256 - alias ;
                        unsigned mixRed = ((alias*fgRed)+(notAlias*bgRed))/256 ;
                        unsigned mixGreen = ((alias*fgGreen)+(notAlias*bgGreen))/256 ;
                        unsigned mixBlue = ((alias*fgBlue)+(notAlias*bgBlue))/256 ;
   
                        unsigned short color = fb.get16( mixRed, mixGreen, mixBlue );
                        fb.getPixel( penX, startY ) = color ;
                     } // don't draw background
                  } // is visible
               } // for each column
            } // on the screen
            rasterLine += bmp.pitch ;
         }
      } // anti-aliased
      else if( 1 == bmp.num_grays )
      {
         unsigned char const *rasterLine = bmp.buffer ;
         for( int row = 0 ; ( row < bmp.rows ) && ( startY < fb.getHeight() ); row++, startY++ )
         {
            if( 0 <= startY )
            {
               unsigned char const *rasterCol = rasterLine ;
               unsigned char mask = 0x80 ;
               int penX = startX ;
               for( int col = 0 ; ( col < bmp.width ) && ( penX < fb.getWidth() ); col++, penX++ )
               {
                  if( 0 <= penX )
                  {
                     unsigned char const alias = ( 0 == ( *rasterCol & mask ) )
                                                 ? 0 
                                                 : 255 ;
                     unsigned short color = fb.get16( alias, alias, alias );
                     fb.getPixel( penX, startY ) = color ;
                  } // is visible
                  
                  mask >>= 1 ;
                  if( 0 == mask )
                  {
                     mask = 0x80 ;
                     rasterCol++ ;
                  }
               } // for each column
            } // on the screen
            rasterLine += bmp.pitch ;
         }
      }
   } // sanity check
}

static JSType const jsTextParamTypes[] = {
   JSTYPE_STRING,    // 0 .ttf font data
   JSTYPE_NUMBER,    // 1 point size
   JSTYPE_NUMBER,    // 2 x position
   JSTYPE_NUMBER,    // 3 y position (baseline)
   JSTYPE_STRING,    // 4 string to render
   JSTYPE_NUMBER,    // 5 fg color (0xFFFFFFFF to invert)
   JSTYPE_NUMBER     // 6 fg color (0xFFFFFFFF for transparent)
};

static unsigned char const jsTextRequired = 5 ;
static unsigned char const jsTextMaxParams = sizeof( jsTextParamTypes ) / sizeof( jsTextParamTypes[0] );

static JSBool
jsText( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( jsTextRequired  <= argc ) 
       &&
       ( jsTextMaxParams >= argc ) )
   {
      int penX = 0 ;
      int penY = 0 ;
      int fontHeight = 0 ;
      bool okay = true ;
      for( int arg = 0 ; okay && ( arg < argc ) ; arg++ )
      {
         JSType const paramType = JS_TypeOfValue( cx, argv[arg] );
         if( paramType != jsTextParamTypes[arg] )
         {
            printf( "Invalid type for arg %d\n", arg );
            okay = false ;
         }
      } // check type of each parameter
      
      if( okay )
      {
         FT_Library library ;
         int error = FT_Init_FreeType( &library );
         if( 0 != error )
         {
            fprintf( stderr, "Error %d initializing freeType library\n", error );
            exit( 3 );
         }

//         initFreeType();

         JSString *fontString = JS_ValueToString( cx, argv[0] );
         if( fontString )
         {
            FT_Face face;      /* handle to face object */

            error = FT_New_Memory_Face( library, 
                                        (FT_Byte *)JS_GetStringBytes( fontString ),   /* first byte in memory */
                                        JS_GetStringLength(fontString),    /* size in bytes        */
                                        0,                          /* face_index           */
                                        &face );
            if( 0 == error )
            {
               int const pointSize = JSVAL_TO_INT( argv[1] );
               if( 0 < pointSize )
               {
                  if( face->face_flags & FT_FACE_FLAG_SCALABLE )
                     error = FT_Set_Char_Size( face, 0, pointSize*64, 80, 80 );
                  else
                     error = 0 ;
                  if( 0 == error )
                  {
                     penX = JSVAL_TO_INT( argv[2] );
                     penY = JSVAL_TO_INT( argv[3] );
                     
                     unsigned long fg = 0xFFFFFF ;
                     unsigned long bg = 0 ;
                     if( argc > 5 )
                     {
                        fg = (unsigned long)JSVAL_TO_INT( argv[5] );
                        if( 6 < argc )
                           bg = (unsigned long)JSVAL_TO_INT( argv[6] );
                     }
                     
                     fontHeight = FT_MulFix( face->height, face->size->metrics.y_scale ) / 64 ;

                     JSString *textString = JS_ValueToString( cx, argv[4] );

                     if( textString )
                     {
                        fbDevice_t &fb = getFB();

                        FT_UInt prevGlyph = 0 ; // used for kerning
                        FT_Bool useKerning = FT_HAS_KERNING( face );

                        jschar const *sText = JS_GetStringChars( textString );
                        while( jschar const c = *sText++ )
                        {
                           // three things can happen here:
                           //    1. we can load the char index and glyph, at which 
                           //       point we can ask about kerning.
                           //    2. we can't load the char index, but can render directly,
                           //       which means we can't kern
                           //    3. everything failed
                           //

                           bool haveGlyph = false ;
                           int  kern = 0 ;

                           FT_UInt glIndex = FT_Get_Char_Index( face, c );
                           if( 0 < glIndex )
                           {
//                              printf( "glyph index for <%c> == %u\n", c, glIndex );
                              error = FT_Load_Glyph( face, glIndex, FT_LOAD_DEFAULT );
                              if( 0 == error )
                              {
                                 if( face->glyph->format != ft_glyph_format_bitmap )
                                 {
//                                    printf( "rendering...\n" );
                                    error = FT_Render_Glyph( face->glyph, ft_render_mode_normal );
                                    if( 0 == error )
                                    {
                                       haveGlyph = true ;
                                       if( useKerning && ( 0 != prevGlyph ) )
                                       {
                                          FT_Vector  delta;
                                          FT_Get_Kerning( face, prevGlyph, glIndex, ft_kerning_default, &delta );
                                          kern = delta.x / 64 ;
//                                          printf( "kerning => %d\n", kern );
                                       }
                                    }
                                    else
                                       fprintf( stderr, "Error %d rendering glyph\n", error );
                                 }
                                 else
                                 {
                                    fprintf( stderr, "Bitmap rendered directly\n" );
                                    haveGlyph = true ;
                                 }
                              }
                              else
                              {
                                 fprintf( stderr, "Error %d loading glyph\n", error );
                              }
                           }
                           else
                           {
//                              printf( "try to load char directly\n" );
                              haveGlyph = ( 0 == FT_Load_Char( face, c, FT_LOAD_RENDER ) );
                           }

                           if( haveGlyph )
                           {
                              // 
                              // Whew! A lot of stuff was done to get here, but now
                              // we have a bitmap (face->glyph->bitmap) (actually a 
                              // byte-map with the image data in 1/256ths
                              //
/*
                              printf( "   rows  %d\n", face->glyph->bitmap.rows );
                              printf( "   width %d\n", face->glyph->bitmap.width );
                              printf( "   pitch %d\n", face->glyph->bitmap.pitch );
                              printf( "  #grays %d\n", face->glyph->bitmap.num_grays );
                              printf( "   pixMode %d\n", face->glyph->bitmap.pixel_mode );
                              printf( "   palMode %d\n", face->glyph->bitmap.palette_mode );
                              
                              printf( "displaying at %d, %d\n",
                                      penX + face->glyph->bitmap_left, 
                                      penY + face->glyph->bitmap_top );
*/

                              render( fb, face->glyph->bitmap, 
                                      penX + face->glyph->bitmap_left, 
                                      penY - face->glyph->bitmap_top,
                                      fg, bg );

                              penX += ( face->glyph->advance.x / 64 );
                              penY += ( face->glyph->advance.y / 64 );

//                              printf( "new position %d, %d\n", penX, penY );
                           }
                           else
                           {
                              fprintf( stderr, "Undefined char <0x%02x>\n", (unsigned char)(c) );
                           }
                           
                           prevGlyph = glIndex ;

                        }
                     }
                     else
                        fprintf( stderr, "Error extracting text string\n" );
                  }
                  else
                     fprintf( stderr, "Error %d setting point size\n", error );
               }
               else
                  fprintf( stderr, "Invalid point size %d\n", pointSize );

               FT_Done_Face( face );
            }
            else
               fprintf( stderr, "Error %d loading face\n", error );
         }
         else
            fprintf( stderr, "Error retrieving font data string\n" );
         
         error = FT_Done_FreeType( library );
         if( 0 != error )
         {
            fprintf( stderr, "Error %d initializing freeType library\n", error );
            exit( 3 );
         }

      }

      *rval = JSVAL_FALSE ;

      JSObject *returnObject = JS_NewObject( cx, 0, 0, obj );
      if( returnObject )
      {
         if( JS_DefineProperty( cx, returnObject, "x", INT_TO_JSVAL(penX), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE )
             &&
             JS_DefineProperty( cx, returnObject, "y", INT_TO_JSVAL(penY), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE )
             &&
             JS_DefineProperty( cx, returnObject, "height", INT_TO_JSVAL(fontHeight), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE ) )
         {
            *rval = OBJECT_TO_JSVAL(returnObject);
         }
      }
         
      return JS_TRUE ;
   
   } // have required # of params
   else
      printf( "Invalid parameter count\n" );

   return JS_FALSE ;
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

static JSBool
jsDumpFont( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) && ( JSTYPE_STRING == JS_TypeOfValue( cx, argv[0] ) ) )
   {
      printf( "params check out\n" );
      
      FT_Library library ;
      int error = FT_Init_FreeType( &library );
      if( 0 != error )
      {
         fprintf( stderr, "Error %d initializing freeType library\n", error );
         exit( 3 );
      }

//      initFreeType();
      printf( "freeType initialized\n" );
      JSString *fontString = JS_ValueToString( cx, argv[0] );
      if( fontString )
      {
         FT_Face face;      /* handle to face object */

         error = FT_New_Memory_Face( library, 
                                     (FT_Byte *)JS_GetStringBytes( fontString ),   /* first byte in memory */
                                     JS_GetStringLength(fontString),    /* size in bytes        */
                                     0,                          /* face_index           */
                                     &face );
         if( 0 == error )
         {
            printf( "num_faces   %ld\n", face->num_faces );
            printf( "face_index    %ld\n", face->face_index );

            printf( "face_flags  %ld:\n", face->face_flags );
            for( int mask = 1, idx = 0 ; mask <= FT_FACE_FLAG_GLYPH_NAMES ; mask <<= 1, idx++ )
            {
               if( idx < numFaceFlags_ )
               {
                  if( 0 != ( face->face_flags & mask ) )
                     printf( "   " );
                  else
                     printf( "   !" );
                  printf( "%s\n", faceFlagNames_[idx] );
               }
               else
                  fprintf( stderr, "How did we get here!\n" );
            } 

            printf( "style_flags  %ld\n", face->style_flags );

            printf( "num_glyphs  %ld\n", face->num_glyphs );

            printf( "family_name %s\n", face->family_name );
            printf( "style_name  %s\n", face->style_name );

            printf( "num_fixed_sizes   %d\n", face->num_fixed_sizes );
            for( int fs = 0 ; fs < face->num_fixed_sizes ; fs++ )
            {
               printf( "   %d/%d\n", face->available_sizes[fs].width,
                                     face->available_sizes[fs].height );
            }
//    FT_Bitmap_Size*   available_sizes;

            printf( "num_charmaps   %d\n", face->num_charmaps );
//    FT_CharMap*       charmaps;

//    FT_Generic        generic;

    // the following are only relevant to scalable outlines
//    FT_BBox           bbox;

            printf( "units_per_EM   %d\n", face->units_per_EM );
            printf( "ascender;      %d\n", face->ascender );
            printf( "descender      %d\n", face->descender );
            printf( "height         %d\n", face->height );

            printf( "max_advance_width %d\n", face->max_advance_width );
            printf( "max_advance_height %d\n", face->max_advance_height );

            printf( "underline_position   %d\n", face->underline_position );
            printf( "underline_thickness  %d\n", face->underline_thickness );

//    FT_GlyphSlot      glyph;
//    FT_Size           size;
//    FT_CharMap        charmap;

 // private begin

//    FT_Driver         driver;
//    FT_Memory         memory;
//    FT_Stream         stream;

//    FT_ListRec        sizes_list;

//    FT_Generic        autohint;
//    void*             extensions;

//    FT_Face_Internal  internal;

for( int cm = 0; cm < face->num_charmaps; cm++ )
{
   FT_CharMap charmap = face->charmaps[cm];
   printf( "charmap %d =>\n", cm );
   printf( "   encoding 0x%08x\n", charmap->encoding );
   printf( "   platform 0x%02x\n", charmap->platform_id );
   printf( "   encodeId 0x%02x\n", charmap->encoding_id );

   FT_UInt   gindex;                                                
   FT_ULong  charcode = FT_Get_First_Char( face, &gindex );
   while( gindex != 0 )                                            
   {                                                                
      printf( "   -> charcode %lu, glyph %d\n", charcode, gindex );
      charcode = FT_Get_Next_Char( face, charcode, &gindex );        
   }                                                                
}

for( int fs = 0 ; fs < face->num_fixed_sizes ; fs++ )
{
   FT_Bitmap_Size const *bms = face->available_sizes + fs;
   printf( "fixed bitmap size %d =>\n", fs );
   printf( "   height %d\n", bms->height );
   printf( "   width  %d\n", bms->width );
}

            FT_Done_Face( face );
         }
         else
            fprintf( stderr, "Error %d loading face\n", error );
      }
      else
         fprintf( stderr, "Error retrieving font data string\n" );
      
      error = FT_Done_FreeType( library );
      if( 0 != error )
      {
         fprintf( stderr, "Error %d freeing freeType library\n", error );
         exit( 3 );
      }
      
      *rval = JSVAL_FALSE ;
      return JS_TRUE ;
   
   } // have required # of params
   else
      printf( "Invalid parameter count\n" );

   return JS_FALSE ;
}

static JSFunctionSpec text_functions[] = {
    {"jsText",           jsText,        1 },
    {"jsDumpFont",       jsDumpFont,    1 },
    {0}
};


bool initJSText( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, text_functions);
}

