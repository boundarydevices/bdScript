/*
 * Module jsBitmap.cpp
 *
 * This module defines the initialization routine
 * and the member routines for the Javascript bitmap
 * class as declared in jsBitmap.h
 *
 *
 * Change History : 
 *
 * $Log: jsBitmap.cpp,v $
 * Revision 1.2  2004-07-04 21:34:58  ericn
 * -added line, rect, textBox, and conversions
 *
 * Revision 1.1  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "jsBitmap.h"
#include "js/jscntxt.h"
#include "fbDev.h"
#include "jsImage.h"
#include "jsAlphaMap.h"
#include <string.h>
#include "dither.h"
#include "hexDump.h"
#include <assert.h>
#include "ftObjs.h"
#include "bitmap.h"
// #define DEBUGPRINT 1
#include "debugPrint.h"

static JSObject *bitmapProto = 0 ;

static JSBool
jsBitmapDraw( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 3 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] ) )
   {
      fbDevice_t &fb = getFB();

      int const specX  = JSVAL_TO_INT( argv[0] );
      int const specY  = JSVAL_TO_INT( argv[1] );
      bool const black = ( 0 == JSVAL_TO_INT( argv[2] ) );

      jsval     vPixMap ;
      jsval     vWidth ;
      jsval     vHeight ;
      JSString *sPixMap ;

      if( JS_GetProperty( cx, obj, "pixBuf", &vPixMap )
          &&
          JSVAL_IS_STRING( vPixMap )
          &&
          ( 0 != ( sPixMap = JSVAL_TO_STRING( vPixMap ) ) )
          &&
          JS_GetProperty( cx, obj, "width", &vWidth )
          &&
          JSVAL_IS_INT( vWidth )
          &&
          JS_GetProperty( cx, obj, "height", &vHeight )
          &&
          JSVAL_IS_INT( vHeight ) )
      {
         unsigned const bmWidth    = JSVAL_TO_INT( vWidth );
         unsigned const bmHeight   = JSVAL_TO_INT( vHeight );
/*
         printf( "string gets drawn here\n"
                 "x        %u\n"
                 "y        %u\n"
                 "rgb      0x%06x\n"
                 "w        %u\n"
                 "h        %u\n", specX, specY, rgb, bmWidth, bmHeight );
*/                 
         //
         // user specifies position of baseline, adjust to top of bitmap
         //
         unsigned bmStartY = 0 ;
         int screenStartY = specY ;
         if( 0 > screenStartY )
         {
            bmStartY = ( 0-screenStartY );
            screenStartY = 0 ;
         } // starts off the screen, walk down

         unsigned bmStartX = 0 ;
         int screenStartX = specX ;
         if( 0 > screenStartX )
         {
            bmStartX = ( 0 - screenStartX );
            screenStartX  = 0 ;
         } // starts off the screen, walk right

         if( ( bmWidth > bmStartX ) && ( screenStartX <= fb.getWidth() ) )
         {
            //
            // point row at start row/col
            //
            unsigned char const *inBits = (unsigned char const *)JS_GetStringBytes( sPixMap );

            //
            // draw every scanline
            //
            unsigned const red   = black ? 0 : 0xFF ;
            unsigned const green = red ;
            unsigned const blue  = red ;

            unsigned screenY = screenStartY ;
            unsigned short const fullColor = fb.get16( red, green, blue );

            for( unsigned bmY = bmStartY ; ( bmY < bmHeight ) && ( screenY < fb.getHeight() ) ; bmY++, screenY++ )
            {
               unsigned bitOffs = bmY * bmWidth + bmStartX ;
               unsigned screenX = screenStartX ;

               for( unsigned bmx = bmStartX ; ( bmx < bmWidth ) && ( screenX < fb.getWidth() ) ; bmx++, screenX++, bitOffs++ )
               {
                  unsigned char byteOffs = bitOffs / 8 ;
                  unsigned char mask = ( 1 << ( bitOffs & 7 ) );
                  bool const bitSet = ( 0 != ( inBits[byteOffs] & mask ) );
                  if( bitSet )
                     fb.setPixel( screenX, screenY, fullColor );
               } // draw entire row
            }
         } // room for something
      }
      else
         JS_ReportError( cx, "Error retrieving bitmap fields" );
   }
   else
      JS_ReportError( cx, "Usage: bitmap.draw( x, y, RGB );" );

   return JS_TRUE ;

}

static JSBool
jsBitmapRect( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 4 <= argc ) 
       &&
       ( 5 >= argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] )
       &&
       JSVAL_IS_INT( argv[3] ) )
   {
      bool const isSet = ( 5 <= argc ) ? ( 0 == JSVAL_TO_INT( argv[4] ) ) : true ;
      unsigned short x1 = (unsigned short)JSVAL_TO_INT( argv[0] );
      unsigned short y1 = (unsigned short)JSVAL_TO_INT( argv[1] );
      unsigned short x2 = (unsigned short)JSVAL_TO_INT( argv[2] );
      unsigned short y2 = (unsigned short)JSVAL_TO_INT( argv[3] );
      jsval widthVal, heightVal, dataVal ;
      if( JS_GetProperty( cx, obj, "width", &widthVal )
          &&
          JS_GetProperty( cx, obj, "height", &heightVal )
          &&
          JS_GetProperty( cx, obj, "pixBuf", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal ) )
      {
         bitmap_t bmp( (unsigned char *)JS_GetStringBytes( JSVAL_TO_STRING(dataVal) ),
                       JSVAL_TO_INT( widthVal ), 
                       JSVAL_TO_INT( heightVal ) );
         bmp.rect( x1, y1, x2-x1+1, y2-y1+1, isSet );
debugPrint( "bmp.rect:%u    %u:%u, %u:%u\n", isSet, x1, y1, x2, y2 );
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "Invalid bitmap" );
   }
   else
      JS_ReportError( cx, "Usage: bmp.rect( x1, y1, x2, y2 [,color=0] );" );

   return JS_TRUE ;
}

static JSBool
jsBitmapLine( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 4 <= argc ) 
       &&
       ( 6 >= argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] )
       &&
       JSVAL_IS_INT( argv[3] ) )
   {
      unsigned char const penWidth = ( 5 <= argc ) ? (unsigned char)JSVAL_TO_INT( argv[4] ) : 1 ;
      unsigned const color = ( 6 <= argc ) ? JSVAL_TO_INT( argv[5] ) : 0 ;
      unsigned short x1 = (unsigned short)JSVAL_TO_INT( argv[0] );
      unsigned short y1 = (unsigned short)JSVAL_TO_INT( argv[1] );
      unsigned short x2 = (unsigned short)JSVAL_TO_INT( argv[2] );
      unsigned short y2 = (unsigned short)JSVAL_TO_INT( argv[3] );
      jsval widthVal, heightVal, dataVal ;
      if( JS_GetProperty( cx, obj, "width", &widthVal )
          &&
          JS_GetProperty( cx, obj, "height", &heightVal )
          &&
          JS_GetProperty( cx, obj, "pixBuf", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal ) )
      {
         bitmap_t bmp( (unsigned char *)JS_GetStringBytes( JSVAL_TO_STRING(dataVal) ),
                       JSVAL_TO_INT( widthVal ), 
                       JSVAL_TO_INT( heightVal ) );
         bmp.line( x1, y1, x2, y2, penWidth, 0 != color );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   else
      JS_ReportError( cx, "Usage: bmp.line( x1, y1, x2, y2 [penWidth=1 [,color=0]] );" );

}

static JSBool
jsTextBox( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   static char const usage_[] = {
      //                    0        1      2  3  4  5      6         7
      "Usage: bmp.textBox( font, pointSize, x, y, w, h, alignment, 'string' )"
   };

   JSObject *fontObj ;
   jsval     fontDataVal ;
   JSString *fontStr ;
   JSString *sText ;
   if( ( 8 == argc )
       &&
       JSVAL_IS_OBJECT( argv[0] )
       &&
       ( 0 != ( fontObj = JSVAL_TO_OBJECT( argv[0] ) ) )
       &&
       JS_GetProperty( cx, fontObj, "data", &fontDataVal )
       &&
       JSVAL_IS_STRING( fontDataVal )
       &&
       ( 0 != ( fontStr = JSVAL_TO_STRING( fontDataVal ) ) )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] )
       &&
       JSVAL_IS_INT( argv[3] )
       &&
       JSVAL_IS_INT( argv[4] ) 
       &&
       JSVAL_IS_INT( argv[5] ) 
       &&
       JSVAL_IS_INT( argv[6] ) 
       &&
       JSVAL_IS_STRING( argv[7] ) 
       &&
       ( 0 != ( sText = JSVAL_TO_STRING( argv[7] ) ) ) )
   {
      unsigned pointSize, x, y, w, h, alignment ;
      pointSize = JSVAL_TO_INT(argv[1]);
      x = JSVAL_TO_INT(argv[2]);
      y = JSVAL_TO_INT(argv[3]);
      w = JSVAL_TO_INT(argv[4]);
      h = JSVAL_TO_INT(argv[5]);
      alignment = JSVAL_TO_INT(argv[6]);
      
      freeTypeFont_t font( JS_GetStringBytes( fontStr ),
                           JS_GetStringLength( fontStr ) );
      if( font.worked() )
      {
         debugPrint( "font loaded\n" );
         jsval widthVal, heightVal, dataVal ;
         if( JS_GetProperty( cx, obj, "width", &widthVal )
             &&
             JS_GetProperty( cx, obj, "height", &heightVal )
             &&
             JS_GetProperty( cx, obj, "pixBuf", &dataVal )
             &&
             JSVAL_IS_STRING( dataVal ) )
         {
            if( freeTypeToBitmapBox( font, pointSize, alignment,
                                     JS_GetStringBytes( sText ),
                                     JS_GetStringLength( sText ),
                                     x, y, w, h,
                                     (unsigned char *)JS_GetStringBytes( JSVAL_TO_STRING(dataVal) ),
                                     bitmap_t::bytesPerRow( JSVAL_TO_INT( widthVal ) ), 
                                     JSVAL_TO_INT( heightVal ) ) )
               *rval = JSVAL_TRUE ;
            else
               JS_ReportError( cx, "Error rendering textBox" );
         }
         else
            JS_ReportError( cx, "Invalid bitmap" );
      }
      else
         JS_ReportError( cx, "parsing font" );
   }
   else
      JS_ReportError( cx, usage_ );

   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static JSFunctionSpec bitmapMethods_[] = {
    {"draw",         jsBitmapDraw,           3 },
    {"rect",         jsBitmapRect,           5 },
    {"line",         jsBitmapLine,           6 },
    {"textBox",      jsTextBox,              8 },
    {0}
};

JSClass jsBitmapClass_ = {
  "bitmap",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

JSPropertySpec bitmapProperties_[BITMAP_NUMPROPERTIES+1] = {
  {"width",         BITMAP_WIDTH,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"height",        BITMAP_HEIGHT,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"pixBuf",        BITMAP_PIXBUF,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

typedef JSBool (*convertToBitmap)(JSContext *cx, JSObject *lhObj, JSObject *rhObj);

static JSBool alphaMapToBitmap(JSContext *cx, JSObject *lhObj, JSObject *rhObj)
{
   jsval     vPixMap ;
   jsval     vWidth ;
   jsval     vHeight ;
   JSString *sAlpha ;

   if( JS_GetProperty( cx, rhObj, "pixBuf", &vPixMap )
       &&
       JSVAL_IS_STRING( vPixMap )
       &&
       ( 0 != ( sAlpha = JSVAL_TO_STRING( vPixMap ) ) )
       &&
       JS_GetProperty( cx, rhObj, "width", &vWidth )
       &&
       JSVAL_IS_INT( vWidth )
       &&
       JS_GetProperty( cx, rhObj, "height", &vHeight )
       &&
       JSVAL_IS_INT( vHeight ) )
   {
      unsigned const rhWidth     = JSVAL_TO_INT( vWidth );
      unsigned const rhHeight    = JSVAL_TO_INT( vHeight );
      if( !JS_DefineProperty( cx, lhObj, "width",    vWidth, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE ) )
         JS_ReportError( cx, "setting width\n" );
      if( !JS_DefineProperty( cx, lhObj, "height",   vHeight, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE ) )
         JS_ReportError( cx, "setting height\n" );
      unsigned char const *alpha = (unsigned char const *)JS_GetStringBytes(sAlpha);
      unsigned const numBits    = rhWidth * rhHeight ;
      unsigned const bitBytes   = (numBits+7)/8 ;

      unsigned char *const bitData = (unsigned char *)JS_malloc( cx, bitBytes );
      if( bitData )
      {
         JSString *sBitmap = JS_NewString( cx, (char *)bitData, bitBytes );
         if( sBitmap )
         {
            JS_DefineProperty( cx, lhObj, "pixBuf", STRING_TO_JSVAL( sBitmap ), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );

            unsigned lhOffs = 0 ;
            for( unsigned y = 0 ; y < rhHeight ; y++ )
            {
               for( unsigned x = 0 ; x < rhWidth ; x++ )
               {
                  unsigned char a = *alpha++ ;
                  unsigned byteOffs = lhOffs / 8 ;
                  unsigned mask     = ( 1 << ( lhOffs & 7 ) );
                  lhOffs++ ;
                  if( a )
                     bitData[byteOffs] |= mask ;
                  else
                     bitData[byteOffs] &= ~mask ;
               }
            }
            memset( bitData, 0xFF, bitBytes );
            return JS_TRUE ;
         }
         else
            JS_ReportError( cx, "Error building bitmap string" );
      }
      else
         JS_ReportError( cx, "Error allocating bitData\n" );
      return JS_TRUE ;
   }
   else
      JS_ReportError( cx, "Invalid alphamap properties" );

   return JS_FALSE ;
   
}

static JSBool bitmapFromImage(JSContext *cx, JSObject *lhObj, JSObject *rhObj)
{
   jsval     vPixMap ;
   jsval     vWidth ;
   jsval     vHeight ;
   JSString *sPixels ;

   if( JS_GetProperty( cx, rhObj, "pixBuf", &vPixMap )
       &&
       JSVAL_IS_STRING( vPixMap )
       &&
       ( 0 != ( sPixels = JSVAL_TO_STRING( vPixMap ) ) )
       &&
       JS_GetProperty( cx, rhObj, "width", &vWidth )
       &&
       JSVAL_IS_INT( vWidth )
       &&
       JS_GetProperty( cx, rhObj, "height", &vHeight )
       &&
       JSVAL_IS_INT( vHeight ) )
   {
      unsigned const rhWidth     = JSVAL_TO_INT( vWidth );
      unsigned const rhHeight    = JSVAL_TO_INT( vHeight );
      if( !JS_DefineProperty( cx, lhObj, "width",    vWidth, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE ) )
         JS_ReportError( cx, "setting width\n" );
      if( !JS_DefineProperty( cx, lhObj, "height",   vHeight, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE ) )
         JS_ReportError( cx, "setting height\n" );
      unsigned const bitsPerRow  = ((rhWidth+31)/32) * 32 ;
      unsigned const bytesPerRow = bitsPerRow / 8 ;
      unsigned const numBits     = bitsPerRow * rhHeight ;
      unsigned const bitBytes    = numBits / 8 ;

      unsigned char *const bitData = (unsigned char *)JS_malloc( cx, bitBytes );
      if( bitData )
      {
         JSString *sBitmap = JS_NewString( cx, (char *)bitData, bitBytes );
         if( sBitmap )
         {
            // root
            JS_DefineProperty( cx, lhObj, "pixBuf", STRING_TO_JSVAL( sBitmap ), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );

            imageToBitmap( cx, rhObj, lhObj, 0, 0 );
            
            return JS_TRUE ;
         }
         else
            JS_ReportError( cx, "Error building bitmap string" );
      }
      else
         JS_ReportError( cx, "Error allocating bitData\n" );
      return JS_TRUE ;
   }
   else
      JS_ReportError( cx, "Invalid alphamap properties" );

   return JS_FALSE ;
   
}

static JSBool bitmapFromBitmap(JSContext *cx, JSObject *lhObj, JSObject *rhObj)
{
   jsval     vPixMap ;
   jsval     vWidth ;
   jsval     vHeight ;
   JSString *sPixels ;

   if( JS_GetProperty( cx, rhObj, "pixBuf", &vPixMap )
       &&
       JSVAL_IS_STRING( vPixMap )
       &&
       ( 0 != ( sPixels = JSVAL_TO_STRING( vPixMap ) ) )
       &&
       JS_GetProperty( cx, rhObj, "width", &vWidth )
       &&
       JSVAL_IS_INT( vWidth )
       &&
       JS_GetProperty( cx, rhObj, "height", &vHeight )
       &&
       JSVAL_IS_INT( vHeight ) )
   {
      unsigned const rhWidth     = JSVAL_TO_INT( vWidth );
      unsigned const rhHeight    = JSVAL_TO_INT( vHeight );
      if( !JS_DefineProperty( cx, lhObj, "width",    vWidth, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE ) )
         JS_ReportError( cx, "setting width\n" );
      if( !JS_DefineProperty( cx, lhObj, "height",   vHeight, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE ) )
         JS_ReportError( cx, "setting height\n" );
      unsigned const bitsPerRow  = ((rhWidth+31)/32) * 32 ;
      unsigned const bytesPerRow = bitsPerRow / 8 ;
      unsigned const numBits     = bitsPerRow * rhHeight ;
      unsigned const bitBytes    = numBits / 8 ;

      unsigned char *const bitData = (unsigned char *)JS_malloc( cx, bitBytes );
      if( bitData )
      {
         JSString *sBitmap = JS_NewString( cx, (char *)bitData, bitBytes );
         if( sBitmap )
         {
            // root
            JS_DefineProperty( cx, lhObj, "pixBuf", STRING_TO_JSVAL( sBitmap ), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );

            memcpy( bitData, JS_GetStringBytes(sPixels), bitBytes );
            
            return JS_TRUE ;
         }
         else
            JS_ReportError( cx, "Error building bitmap string" );
      }
      else
         JS_ReportError( cx, "Error allocating bitData\n" );
      return JS_TRUE ;
   }
   else
      JS_ReportError( cx, "Invalid alphamap properties" );

   return JS_FALSE ;
   
}

static JSBool bitmap( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) 
       && 
       ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject * const rhObj = JSVAL_TO_OBJECT( argv[0] );
      convertToBitmap  convert = 0 ;

      if( JS_InstanceOf( cx, rhObj, &jsAlphaMapClass_, NULL ) )
      {
         convert = alphaMapToBitmap ;
      }
      else if( JS_InstanceOf( cx, rhObj, &jsImageClass_, NULL ) )
      {
         convert = bitmapFromImage ;
      }
      else if( JS_InstanceOf( cx, rhObj, &jsBitmapClass_, NULL ) )
      {
         convert = bitmapFromBitmap ;
      }
      else
         JS_ReportError( cx, "Unknown conversion to bitmap" );

      if( convert )
      {
         JSObject *thisObj = JS_NewObject( cx, &jsBitmapClass_, bitmapProto, NULL );
   
         if( thisObj )
         {
            *rval = OBJECT_TO_JSVAL( thisObj ); // root
            if( !convert( cx, thisObj, rhObj ) )
               JS_ReportError( cx, "Error converting bitmap" );
         }
         else
            JS_ReportError( cx, "Error allocating bitmap" );
      }
   }
   else if( ( 2 == argc )
            &&
            JSVAL_IS_INT( argv[0] )
            &&
            JSVAL_IS_INT( argv[1] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsBitmapClass_, bitmapProto, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         unsigned const rhWidth  = JSVAL_TO_INT( argv[0] );
         unsigned const rhHeight = JSVAL_TO_INT( argv[1] );
         unsigned const bitsPerRow  = ((rhWidth+31)/32) * 32 ;
         unsigned const bytesPerRow = bitsPerRow / 8 ;
         unsigned const bitBytes    = bytesPerRow * rhHeight ;
debugPrint( "%u x %u, %u bytes per row, %u bytes total\n",
        rhWidth, rhHeight, bytesPerRow, bitBytes );
         unsigned char *const bitData = (unsigned char *)JS_malloc( cx, bitBytes );
         if( bitData )
         {
            memset( bitData, 0, bitBytes );
            JSString *sBitmap = JS_NewString( cx, (char *)bitData, bitBytes );
            if( sBitmap )
            {
               // root
               JS_DefineProperty( cx, thisObj, "pixBuf", STRING_TO_JSVAL( sBitmap ), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               if( !JS_DefineProperty( cx, thisObj, "width", argv[0], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE ) )
                  JS_ReportError( cx, "setting width\n" );
               if( !JS_DefineProperty( cx, thisObj, "height", argv[1], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE ) )
                  JS_ReportError( cx, "setting height\n" );
            }
            else
               JS_ReportError( cx, "Error allocating bitmap string" );
         }
         else
            JS_ReportError( cx, "Error allocating bitmap bits" );
      }
      else
         JS_ReportError( cx, "Error allocating bitmap" );
   }
   else
      JS_ReportError( cx, "Usage : new bitmap( alphaMap | image | width, height );" );
      
   return JS_TRUE ;

}

bool initJSBitmap( JSContext *cx, JSObject *glob )
{
   if( 0 == bitmapProto )
   {
      bitmapProto = JS_InitClass( cx, glob, NULL, &jsBitmapClass_,
                                     bitmap, 1,
                                     bitmapProperties_, 
                                     bitmapMethods_,
                                     0, 0 );
      JS_AddRoot( cx, &bitmapProto );
   }
   return ( 0 != bitmapProto );
}


void alphaMapToBitmap( JSContext *cx, 
                   JSObject  *alphaMap,
                   JSObject  *bitmap,
                   unsigned   xStart,
                   unsigned   yStart )
{
}

//
// draw (dither) an image into a bitmap at specified
// location
//
void imageToBitmap( JSContext *cx, 
                JSObject  *image,      // input
                JSObject  *bitmap,     // output
                unsigned   xStart,
                unsigned   yStart )
{
   jsval     vPixMap ;
   jsval     vWidth ;
   jsval     vHeight ;
   JSString *sImagePix ;

   if( JS_GetProperty( cx, image, "pixBuf", &vPixMap )
       &&
       JSVAL_IS_STRING( vPixMap )
       &&
       ( 0 != ( sImagePix = JSVAL_TO_STRING( vPixMap ) ) )
       &&
       JS_GetProperty( cx, image, "width", &vWidth )
       &&
       JSVAL_IS_INT( vWidth )
       &&
       JS_GetProperty( cx, image, "height", &vHeight )
       &&
       JSVAL_IS_INT( vHeight ) )
   {
      unsigned imageWidth     = JSVAL_TO_INT( vWidth );
      unsigned imageHeight    = JSVAL_TO_INT( vHeight );

      JSString *sBitmapBits ;
      if( JS_GetProperty( cx, bitmap, "pixBuf", &vPixMap )
          &&
          JSVAL_IS_STRING( vPixMap )
          &&
          ( 0 != ( sBitmapBits = JSVAL_TO_STRING( vPixMap ) ) )
          &&
          JS_GetProperty( cx, bitmap, "width", &vWidth )
          &&
          JSVAL_IS_INT( vWidth )
          &&
          JS_GetProperty( cx, bitmap, "height", &vHeight )
          &&
          JSVAL_IS_INT( vHeight ) )
      {
         unsigned short const *pixels = (unsigned short const *)JS_GetStringBytes(sImagePix);

         unsigned const bitmapWidth  = JSVAL_TO_INT( vWidth );
         unsigned const bitmapHeight = JSVAL_TO_INT( vHeight );

         //
         // Clip the height to bitmap bounds
         //
         if( ( yStart < bitmapHeight )
             &&
             ( xStart < bitmapWidth ) )
         {
            unsigned maxHeight = bitmapHeight - yStart ;
            if( maxHeight < imageHeight )
               imageHeight = maxHeight ;

// If we want to display part of an image (i.e. clip width), 
// we should add support to that in dither_t and do it here.
            dither_t dither( pixels,  imageWidth, imageHeight );

            unsigned const bitsPerRow   = ((bitmapWidth+31)/32) * 32 ;
            unsigned const bytesPerRow  = bitsPerRow / 8 ;
   
            unsigned char *const bitData = (unsigned char *)JS_GetStringBytes(sBitmapBits)
                                           + (yStart*bytesPerRow)
                                           + (xStart/8);
            unsigned char *const bitEnd = bitData + JS_GetStringLength(sBitmapBits);
            unsigned char const  startOutMask = (0x80 >> (xStart & 7));
            unsigned char const *nextIn = dither.getBits();
            unsigned char        inByte = *nextIn++ ;
   
            unsigned maxWidth = bitmapWidth - xStart ;

            unsigned char inMask = 1 ;

            for( unsigned y = 0 ; y < imageHeight ; y++ )
            {
// 
// If (when) we're clipping the width of the source image,
// we'll need to set nextIn, inByte, and mask here
//
               unsigned char *nextOut = bitData + (y*bytesPerRow) ;
               unsigned outMask = startOutMask ;
               unsigned char out = *nextOut ; // get leading bits

               for( unsigned x = 0 ; x < imageWidth ; x++ )
               {
                  if( inByte & inMask )
                     out &= ~outMask ;
                  else
                     out |= outMask ;
                  
                  inMask <<= 1 ;
                  if( 0 == inMask )
                  {
                     inMask = 1 ;
                     inByte = *nextIn++ ;
                  }
   
                  outMask >>= 1 ;
                  if( 0 == outMask )
                  {
                     outMask = 0x80 ;
                     if( x < maxWidth )
                        *nextOut++ = out ;
                     out = *nextOut ; // trailing bits
                  }
               }

               assert( nextOut <= bitEnd );
            }
         } // clip to max y
      }
      else
         JS_ReportError( cx, "draw img->bitmap: Invalid bitmap\n" );
   }
   else
      JS_ReportError( cx, "draw img->bitmap: Invalid source image\n" );
}

