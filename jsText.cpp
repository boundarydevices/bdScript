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
 * Revision 1.12  2002-12-15 20:01:09  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.11  2002/12/03 13:36:13  ericn
 * -collapsed code and objects for curl transfers
 *
 * Revision 1.10  2002/11/30 16:26:50  ericn
 * -better error checking, new curl interface
 *
 * Revision 1.9  2002/11/30 05:30:16  ericn
 * -modified to expect call from default curl hander to app-specific
 *
 * Revision 1.8  2002/11/30 00:31:29  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.7  2002/11/05 05:40:55  ericn
 * -pre-declare font::isLoaded()
 *
 * Revision 1.6  2002/11/03 17:55:51  ericn
 * -modified to support synchronous gets and posts
 *
 * Revision 1.5  2002/11/02 18:57:03  ericn
 * -removed debug stuff
 *
 * Revision 1.4  2002/11/02 18:38:02  ericn
 * -added font.render()
 *
 * Revision 1.3  2002/11/02 04:11:43  ericn
 * -added font class
 *
 * Revision 1.2  2002/10/20 16:31:06  ericn
 * -added bg color support
 *
 * Revision 1.1  2002/10/18 01:18:25  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsText.h"
#include "ftObjs.h"
#include <stdio.h>
#include "fbDev.h"
#include <ctype.h>
#include "jsCurl.h"
#include "js/jscntxt.h"
#include "jsGlobals.h"
#include "jsCurl.h"
#include "jsAlphaMap.h"

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
                              // byte-map with the image data in 1/255ths
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
jsFontDump( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   jsval     dataVal ;
   JSString *fontString ; 

   if( JS_GetProperty( cx, obj, "data", &dataVal )
       &&
       JSVAL_IS_STRING( dataVal )
       &&
       ( 0 != ( fontString = JSVAL_TO_STRING( dataVal ) ) ) )
   {
      freeTypeLibrary_t library ;
      freeTypeFont_t    font( library, 
                              JS_GetStringBytes( fontString ),
                              JS_GetStringLength( fontString ) );
      if( font.worked() )
      {
         FT_Face face = font.face_ ;
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
               JS_ReportError( cx, "How did we get here!\n" );
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

         printf( "num_charmaps   %d\n", face->num_charmaps );


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

         for( int cm = 0; cm < face->num_charmaps; cm++ )
         {
            FT_CharMap charmap = face->charmaps[cm];
            printf( "charmap %d =>\n", cm );
            printf( "   encoding 0x%08x\n", charmap->encoding );
            printf( "   platform 0x%02x\n", charmap->platform_id );
            printf( "   encodeId 0x%02x\n", charmap->encoding_id );
         
            unsigned long numCharCodes = 0 ;

#define BADCHARCODE 0xFFFFaaaa
            
            FT_ULong  startCharCode = BADCHARCODE ;
            FT_ULong  prevCharCode = BADCHARCODE ;

            FT_UInt   gindex;                                                
            FT_ULong  charcode = FT_Get_First_Char( face, &gindex );
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
               charcode = FT_Get_Next_Char( face, charcode, &gindex );        
            }                                                                
            
            if( BADCHARCODE != prevCharCode )
               numCharCodes += ( prevCharCode-startCharCode ) + 1 ;

            printf( "   -> %lu charcodes\n", numCharCodes );
         }
         
         for( int fs = 0 ; fs < face->num_fixed_sizes ; fs++ )
         {
            FT_Bitmap_Size const *bms = face->available_sizes + fs;
            printf( "fixed bitmap size %d =>\n", fs );
            printf( "   height %d\n", bms->height );
            printf( "   width  %d\n", bms->width );
         }
      }
      else
         JS_ReportError( cx, "parsing font\n" );
   }
   else
      JS_ReportError( cx, "reading font data property" );
      
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static bool addRange( JSContext *cx, 
                      JSObject  *arrObj, 
                      FT_ULong  rangeStart,
                      FT_ULong  rangeEnd )
{
   jsuint numRanges ;
   if( JS_GetArrayLength(cx, arrObj, &numRanges ) )
   {
      jsval range[2];
      range[0] = INT_TO_JSVAL( rangeStart );
      range[1] = INT_TO_JSVAL( rangeEnd );
   
      if( JS_SetArrayLength( cx, arrObj, numRanges+1 ) )
      {
         JSObject *rangeObj = JS_NewArrayObject( cx, 2, range );
         if( rangeObj )
         {
            if( JS_DefineElement( cx, arrObj, numRanges, 
                                  OBJECT_TO_JSVAL( rangeObj ),
                                  0, 0, 
                                  JSPROP_ENUMERATE
                                 |JSPROP_PERMANENT
                                 |JSPROP_READONLY ) )
            {
               return true ;
            }
            else
               JS_ReportError( cx, "Error adding array element" );
         }
         else
            JS_ReportError( cx, "Error allocating new range" );
      }
      else
         JS_ReportError( cx, "Error changing array length" );
   }

   return false ;

}

static JSBool
jsFontCharCodes( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   jsval     dataVal ;
   JSString *fontString ; 

   if( JS_GetProperty( cx, obj, "data", &dataVal )
       &&
       JSVAL_IS_STRING( dataVal )
       &&
       ( 0 != ( fontString = JSVAL_TO_STRING( dataVal ) ) ) )
   {
      freeTypeLibrary_t library ;
      freeTypeFont_t    font( library, 
                              JS_GetStringBytes( fontString ),
                              JS_GetStringLength( fontString ) );
      if( font.worked() )
      {
         JSObject *arrObj = JS_NewArrayObject( cx, 0, 0 );
         if( arrObj )
         {
            *rval = OBJECT_TO_JSVAL( arrObj );

            FT_ULong  startCharCode = BADCHARCODE ;
            FT_ULong  prevCharCode = BADCHARCODE ;
   
            FT_UInt   gindex;                                                
            FT_ULong  charcode = FT_Get_First_Char( font.face_, &gindex );
            while( gindex != 0 )                                            
            {                                                                
               if( charcode != prevCharCode + 1 )
               {
                  if( BADCHARCODE != prevCharCode )
                  {
                     if( !addRange( cx, arrObj, startCharCode, prevCharCode ) )
                        break;
                  }
                  startCharCode = prevCharCode = charcode ;
               }
               else
                  prevCharCode = charcode ;
               
               charcode = FT_Get_Next_Char( font.face_, charcode, &gindex );        
            }                                                                
            
            if( BADCHARCODE != prevCharCode )
               addRange( cx, arrObj, startCharCode, prevCharCode );
         }
         else
            JS_ReportError( cx, "allocating array" );
      }
      else
         JS_ReportError( cx, "parsing font\n" );
   }
   else
      JS_ReportError( cx, "reading font data property" );
   
   return JS_TRUE ;
}

static JSBool
jsFontRender( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( ( 2 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_STRING( argv[1] ) )
   {
      jsval     dataVal ;
      JSString *fontString ; 
   
      if( JS_GetProperty( cx, obj, "data", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal )
          &&
          ( 0 != ( fontString = JSVAL_TO_STRING( dataVal ) ) ) )
      {
         freeTypeLibrary_t library ;
         freeTypeFont_t    font( library, 
                                 JS_GetStringBytes( fontString ),
                                 JS_GetStringLength( fontString ) );
         if( font.worked() )
         {
            unsigned  pointSize = JSVAL_TO_INT( argv[0] );
            JSString *sParam = JSVAL_TO_STRING( argv[1] );
            
            char const *renderString = JS_GetStringBytes( sParam );
            unsigned const renderLen = JS_GetStringLength( sParam );
   
            freeTypeString_t ftString( font, pointSize, renderString, renderLen );
            
            JSObject *returnObj = JS_NewObject( cx, &jsAlphaMapClass_, 0, 0 );
            if( returnObj )
            {
               *rval = OBJECT_TO_JSVAL( returnObj );
               JSString *sAlphaMap = JS_NewStringCopyN( cx, (char const *)ftString.getRow(0), ftString.getDataLength() );
               if( sAlphaMap )
               {
                  JS_DefineProperty( cx, returnObj, "pixBuf", STRING_TO_JSVAL( sAlphaMap ), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               }
               else
                  JS_ReportError( cx, "Error building alpha map string" );
   
               JS_DefineProperty( cx, returnObj, "width",    INT_TO_JSVAL(ftString.getWidth()), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               JS_DefineProperty( cx, returnObj, "height",   INT_TO_JSVAL(ftString.getHeight()), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               JS_DefineProperty( cx, returnObj, "baseline", INT_TO_JSVAL(ftString.getBaseline()), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               JS_DefineProperty( cx, returnObj, "yAdvance", INT_TO_JSVAL(ftString.getFontHeight()), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
            }
            else
               JS_ReportError( cx, "allocating array" );
         }
         else
            JS_ReportError( cx, "parsing font\n" );
      }
      else
         JS_ReportError( cx, "reading font data property" );
   }
   else
      JS_ReportError( cx, "Usage: font.render( pointSize, 'string' );" );
   
   return JS_TRUE ;
}

static JSFunctionSpec fontMethods[] = {
    {"dump",       jsFontDump,              0 },
    {"charCodes",  jsFontCharCodes,         0 },
    {"render",     jsFontRender,            0 },
    {0}
};


enum jsFont_tinyId {
   FONT_ISLOADED,
   FONT_DATA
};


extern JSClass jsFontClass_ ;

JSClass jsFontClass_ = {
  "font",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec fontProperties_[] = {
  {"isLoaded",    FONT_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"data",        FONT_DATA,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static void fontOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   freeTypeLibrary_t library ;
   std::string sError ;
   freeTypeFont_t font( library, data, size );
   if( font.worked() )
   {
      JSString *fontString = JS_NewStringCopyN( req.cx_, (char const *)data, size );
      if( fontString )
      {
         JS_DefineProperty( req.cx_, req.lhObj_, "data",
                            STRING_TO_JSVAL( fontString ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
      }
      else
         sError = "Error allocating font string" ;
   }
   else
      sError = "Error loading font face" ;
         
   if( 0 < sError.size() )
   {
   }
}

static JSBool font( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsFontClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         
         if( queueCurlRequest( thisObj, argv[0], cx, fontOnComplete ) )
         {
            return JS_TRUE ;
         }
         else
         {
            JS_ReportError( cx, "Error queueing curlRequest" );
         }
      }
      else
         JS_ReportError( cx, "Error allocating curlFile" );
   }
   else
      JS_ReportError( cx, "Usage : new font( { url:\"something\" [,useCache:true] [,onLoad=\"doSomething();\"][,onLoadError=\"somethingElse();\"] } );" );
      
   return JS_TRUE ;

}

static JSFunctionSpec text_functions[] = {
    {"jsText",           jsText,        1 },
    {0}
};


bool initJSText( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsFontClass_,
                                  font, 1,
                                  fontProperties_, 
                                  fontMethods,
                                  0, 0 );
   if( rval )
   {
      return JS_DefineFunctions( cx, glob, text_functions);
   }
   
   return false ;
}

