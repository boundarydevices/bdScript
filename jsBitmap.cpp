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
 * Revision 1.1  2004-03-17 04:56:19  ericn
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


static JSFunctionSpec bitmapMethods_[] = {
    {"draw",         jsBitmapDraw,           3 },
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

static JSBool imageToBitmap(JSContext *cx, JSObject *lhObj, JSObject *rhObj)
{
   return JS_TRUE ;
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
//         convert = imageToBitmap ;
      }
      else if( JS_InstanceOf( cx, rhObj, &jsImageClass_, NULL ) )
      {
         convert = imageToBitmap ;
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
   else
      JS_ReportError( cx, "Usage : new bitmap( alphaMap | image );" );
      
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
