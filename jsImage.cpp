/*
 * Module jsImage.cpp
 *
 * This module defines the JavaScript routines
 * which display images as described and declared 
 * in jsImage.h
 *
 *
 * Change History : 
 *
 * $Log: jsImage.cpp,v $
 * Revision 1.39  2009-02-16 23:04:09  valli
 * defined jsImageBox function.
 * added method to set alpha. modified methods that modify pixel data (jsSetPixel,
 * jsImageRect, and jsImageLine).
 *
 * Revision 1.38  2007-07-03 18:07:14  ericn
 * -added parameter for Scale16::scale
 *
 * Revision 1.37  2006/12/02 14:21:38  ericn
 * -updated usage for image.draw()
 *
 * Revision 1.36  2006/11/22 17:26:28  ericn
 * -add imageToPS global method
 *
 * Revision 1.35  2006/10/29 21:56:26  ericn
 * -add imageInfo() function
 *
 * Revision 1.34  2006/03/28 04:16:50  ericn
 * -conditional compile on Cairo
 *
 * Revision 1.33  2005/12/11 16:02:30  ericn
 * -
 *
 * Revision 1.32  2005/11/27 16:18:09  ericn
 * -added image.getPixel()
 *
 * Revision 1.31  2005/11/06 00:49:31  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.30  2004/07/04 21:32:25  ericn
 * -added bitmap->bitmap construction
 *
 * Revision 1.29  2004/05/08 16:33:50  ericn
 * -removed debug msg
 *
 * Revision 1.28  2004/05/08 14:23:02  ericn
 * -added drawing primitives to image object
 *
 * Revision 1.27  2004/05/05 03:19:37  ericn
 * -added draw into image
 *
 * Revision 1.26  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.25  2003/06/26 08:03:34  tkisky
 * -add rotate90 function
 *
 * Revision 1.24  2003/05/18 10:18:23  tkisky
 * -add scaling function
 *
 * Revision 1.23  2003/04/26 15:42:57  ericn
 * -added method dither()
 *
 * Revision 1.22  2003/03/13 14:46:51  ericn
 * -added method dissolve()
 *
 * Revision 1.21  2002/12/15 20:01:30  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.20  2002/12/03 13:36:13  ericn
 * -collapsed code and objects for curl transfers
 *
 * Revision 1.19  2002/11/30 16:26:00  ericn
 * -better error checking, new curl interface
 *
 * Revision 1.18  2002/11/30 05:30:16  ericn
 * -modified to expect call from default curl hander to app-specific
 *
 * Revision 1.17  2002/11/30 00:31:24  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.16  2002/11/23 16:29:15  ericn
 * -added alpha channel support for PNG and GIF
 *
 * Revision 1.15  2002/11/22 21:31:43  tkisky
 * -Optimize render and use it in jsImage
 *
 * Revision 1.14  2002/11/22 15:08:03  ericn
 * -made jsImageDraw public
 *
 * Revision 1.13  2002/11/20 00:39:02  ericn
 * -fixed memory leak
 *
 * Revision 1.12  2002/11/11 04:28:55  ericn
 * -moved headers
 *
 * Revision 1.11  2002/11/08 13:58:23  ericn
 * -modified to handle negative screen positions
 *
 * Revision 1.10  2002/11/05 05:41:19  ericn
 * -pre-declare image::isLoaded
 *
 * Revision 1.9  2002/11/03 17:55:51  ericn
 * -modified to support synchronous gets and posts
 *
 * Revision 1.8  2002/11/03 17:03:22  ericn
 * -added synchronous image loads
 *
 * Revision 1.7  2002/11/02 18:38:14  ericn
 * -made jsImageClass_ externally visible
 *
 * Revision 1.6  2002/10/31 02:10:58  ericn
 * -modified image() constructor to be multi-threaded, use rh object
 *
 * Revision 1.5  2002/10/25 13:47:28  ericn
 * -fixed memory leak, removed debug code
 *
 * Revision 1.4  2002/10/25 04:50:58  ericn
 * -removed debug statements
 *
 * Revision 1.3  2002/10/16 02:03:19  ericn
 * -Added GIF support
 *
 * Revision 1.2  2002/10/15 05:00:14  ericn
 * -added imageJPEG routine
 *
 * Revision 1.1  2002/10/13 16:32:25  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "config.h"
#include "jsImage.h"
#include "fbDev.h"
#include "hexDump.h"
#include "js/jscntxt.h"
#include "imgGIF.h"
#include "imgPNG.h"
#include "imgJPEG.h"
#include "jsCurl.h"
#include "jsGlobals.h"
#include "jsAlphaMap.h"
#include "bdGraph/Scale16.h"
#include "dither.h"
#include "jsBitmap.h"
#include "imageInfo.h"
#include "imageToPS.h"

#if CONFIG_JSCAIRO == 1
#include "jsCairo.h"
#include <cairo.h>
#endif

JSBool
jsImageDraw( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   //
   // need x and y [ and optional image destination ]
   //
   JSObject *destObj = 0 ;
   if( ( 2 <= argc )
       &&
       ( 3 >= argc )
       &&
       JSVAL_IS_INT( argv[0] )   // x
       &&
       JSVAL_IS_INT( argv[1] )   // y
       &&
       ( ( 2 == argc )
         ||
         ( JSVAL_IS_OBJECT( argv[2] ) 
           &&
           ( 0 != ( destObj = JSVAL_TO_OBJECT( argv[2] ) ) ) ) ) )
   {
      int xPos = JSVAL_TO_INT( argv[0] );
      int yPos = JSVAL_TO_INT( argv[1] );

//      JS_ReportError( cx, "x:%d, y:%d", xPos, yPos );

      jsval widthVal, heightVal, dataVal ;

      if( JS_GetProperty( cx, obj, "width", &widthVal )
          &&
          JS_GetProperty( cx, obj, "height", &heightVal )
          &&
          JS_GetProperty( cx, obj, "pixBuf", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal ) )
      {
         int srcWidth  = JSVAL_TO_INT( widthVal ); 
         int srcHeight = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short const *const srcPixels = (unsigned short *)JS_GetStringBytes( pixStr );
         if( JS_GetStringLength( pixStr ) == srcWidth * srcHeight * sizeof( srcPixels[0] ) )
         {
            jsval     alphaVal ;
            JSString *sAlpha ;
            unsigned char const *alpha = 0 ;

            if( JS_GetProperty( cx, obj, "alpha", &alphaVal ) 
                &&
                JSVAL_IS_STRING( alphaVal )
                &&
                ( 0 != ( sAlpha = JSVAL_TO_STRING( alphaVal ) ) ) )
            {
               alpha = (unsigned char const *)JS_GetStringBytes( sAlpha );
            } // have alpha channel... use it

            fbDevice_t &fb = getFB();
            if( 0 == destObj )
            {
               fb.render(xPos,yPos,srcWidth,srcHeight,srcPixels, alpha );
            } // draw to screen
            else if( JS_InstanceOf( cx, destObj, &jsImageClass_, NULL ) )
            {
               if( JS_GetProperty( cx, destObj, "width", &widthVal )
                   &&
                   JS_GetProperty( cx, destObj, "height", &heightVal )
                   &&
                   JS_GetProperty( cx, destObj, "pixBuf", &dataVal )
                   &&
                   JSVAL_IS_STRING( dataVal ) )
               {
                  int destWidth  = JSVAL_TO_INT( widthVal ); 
                  int destHeight = JSVAL_TO_INT( heightVal );
                  pixStr = JSVAL_TO_STRING( dataVal );
                  unsigned short *const destPixels = (unsigned short *)JS_GetStringBytes( pixStr );
                  if( JS_GetStringLength( pixStr ) == destWidth * destHeight * sizeof( destPixels[0] ) )
                  {   
                     fbDevice_t::render( xPos, yPos, srcWidth, srcHeight, srcPixels, alpha, destPixels, destWidth, destHeight );
                  }
                  else
                     JS_ReportError( cx, "Invalid destination pixels" );
               }
               else
                  JS_ReportError( cx, "Invalid destination image" );
            }
            else if( JS_InstanceOf( cx, destObj, &jsBitmapClass_, NULL ) )
            {
               imageToBitmap( cx, obj, destObj, xPos, yPos );
            }
            else
               JS_ReportError( cx, "Don't know how to draw into this object" );
            
            *rval = JSVAL_TRUE ;
      
         }
         else
            JS_ReportError( cx, "Invalid srcPixels" );

//         JS_ReportError( cx, "w:%d, h:%d", width, height );
      }
      else
         JS_ReportError( cx, "Object not initialized, can't draw" );

   } // need at least two params
   else
   {
      JS_ReportError( cx, "Usage: image().draw( x, y [,image|bitmap] )" );
      JS_ReportError( cx, "#args == %d", argc );
   }

   return JS_TRUE ;
}

JSBool
jsImageDither( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   jsval widthVal, heightVal, dataVal ;
   if( JS_GetProperty( cx, obj, "width", &widthVal )
       &&
       JS_GetProperty( cx, obj, "height", &heightVal )
       &&
       JS_GetProperty( cx, obj, "pixBuf", &dataVal )
       &&
       JSVAL_IS_STRING( dataVal ) )
   {
      int const width  = JSVAL_TO_INT( widthVal );
      int const height = JSVAL_TO_INT( heightVal );
      JSString *pixStr = JSVAL_TO_STRING( dataVal );
      unsigned short const *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
      if( JS_GetStringLength( pixStr ) == width * height * sizeof( pixMap[0] ) )
      {
         JSObject *returnObj = JS_NewObject( cx, &jsAlphaMapClass_, 0, 0 );
         if( returnObj )
         {
            *rval = OBJECT_TO_JSVAL( returnObj ); // root
            unsigned const pixBytes = width*height ;
            void *const pixMem = JS_malloc( cx, pixBytes );
            JSString *sAlphaMap = JS_NewString( cx, (char *)pixMem, pixBytes );
            if( sAlphaMap )
            {
               JS_DefineProperty( cx, returnObj, "pixBuf", STRING_TO_JSVAL( sAlphaMap ), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               dither_t dither( pixMap, width, height );
               unsigned char *const alphaOut = (unsigned char *)pixMem ;
               for( unsigned y = 0 ; y < (unsigned)height ; y++ )
               {
                  for( unsigned x = 0 ; x < (unsigned)width ; x++ )
                  {
                     alphaOut[y*width+x] = dither.isBlack( x, y ) ? 0 : 0xFF ;
                  }
               }
            }
            else
               JS_ReportError( cx, "Error building alpha map string" );

            JS_DefineProperty( cx, returnObj, "width",    INT_TO_JSVAL(width), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "height",   INT_TO_JSVAL(height), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
         }
         else
            JS_ReportError( cx, "allocating array" );
      }
      else
         JS_ReportError( cx, "Invalid width or height" );
   }
   else
      JS_ReportError( cx, "Invalid image" );

   return JS_TRUE ;
}

JSBool
jsImageScale( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int destWidth  = 0;
   int destHeight = 0;
   int srcLeft    = 0;
   int srcTop     = 0;
   int srcWidth   = 0;
   int srcHeight  = 0;

   int i = (argc>>1) - 1;
   if (i>=0) {
       if ( JSVAL_IS_INT( argv[0] ) && JSVAL_IS_INT( argv[1] ) ) {
               destWidth  = JSVAL_TO_INT( argv[0] );
               destHeight = JSVAL_TO_INT( argv[1] );
       } else i=-1;
       if (i>=1) {
           if (JSVAL_IS_INT( argv[2] ) && JSVAL_IS_INT( argv[3] ) ) {
               srcLeft    = JSVAL_TO_INT( argv[2] );
               srcTop     = JSVAL_TO_INT( argv[3] );
           } else i=-1;
       }
       if (i>=2) {
           if (JSVAL_IS_INT( argv[4] ) && JSVAL_IS_INT( argv[5] ) ) {
               srcWidth   = JSVAL_TO_INT( argv[4] );
               srcHeight  = JSVAL_TO_INT( argv[5] );
           } else i=-1;
       }
   }
   if ((argc & 1) || (i>2) || (i<0)) {
      JS_ReportError( cx, "Usage: img.scale( desiredWidth, desiredHeight [,srcLeft,srcTop[,srcWidth, srcHeight]] );" );
   }
   else
   {
      jsval widthVal, heightVal, dataVal ;
      if( JS_GetProperty( cx, obj, "width", &widthVal )
          &&
          JS_GetProperty( cx, obj, "height", &heightVal )
          &&
          JS_GetProperty( cx, obj, "pixBuf", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal ) )
      {
         int const width  = JSVAL_TO_INT( widthVal );
         int const height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short const *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         if( JS_GetStringLength( pixStr ) == width * height * sizeof( pixMap[0] ) )
         {
            JSObject *returnObj = JS_NewObject( cx, &jsImageClass_, 0, 0 );
            if( returnObj )
            {
               *rval = OBJECT_TO_JSVAL( returnObj ); // root

               if ((srcWidth <=0)||(srcWidth >(width-srcLeft))) srcWidth = width-srcLeft;
               if ((srcHeight<=0)||(srcHeight>(height-srcTop))) srcHeight = height-srcTop;

               unsigned const pixBytes = destWidth*destHeight*sizeof( short );
               void *const pixMem = JS_malloc( cx, pixBytes );
               if (pixMem)
               {
                  Scale16::scale( (unsigned short *)pixMem, destWidth, destHeight, destWidth*sizeof(short),
				  pixMap, width,height, srcLeft,srcTop,srcWidth,srcHeight);

                  JSString *sScaleMap = JS_NewString( cx, (char *)pixMem, pixBytes );
                  if( sScaleMap )
                  {
                     JS_DefineProperty( cx, returnObj, "pixBuf", STRING_TO_JSVAL( sScaleMap ), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
                     JS_DefineProperty( cx, returnObj, "width",    INT_TO_JSVAL(destWidth), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
                     JS_DefineProperty( cx, returnObj, "height",   INT_TO_JSVAL(destHeight), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
                  }
                  else
                     JS_ReportError( cx, "Error building Scale map string" );
               }
               else
                  JS_ReportError( cx, "Error allocating Scale map string" );
            }
            else
               JS_ReportError( cx, "allocating array" );
         }
         else
            JS_ReportError( cx, "Invalid width or height" );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   return JS_TRUE ;
}

JSBool
jsImageRotate90( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if (argc) {
      JS_ReportError( cx, "Usage: img.rotate90();" );
   }
   else
   {
      jsval widthVal, heightVal, dataVal ;
      if( JS_GetProperty( cx, obj, "width", &widthVal )
          &&
          JS_GetProperty( cx, obj, "height", &heightVal )
          &&
          JS_GetProperty( cx, obj, "pixBuf", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal ) )
      {
         int const width  = JSVAL_TO_INT( widthVal );
         int const height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short const *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         unsigned const pixBytes = width * height * sizeof( pixMap[0] );
         if( JS_GetStringLength( pixStr ) == pixBytes )
         {
            JSObject *returnObj = JS_NewObject( cx, &jsImageClass_, 0, 0 );
            if( returnObj )
            {
               *rval = OBJECT_TO_JSVAL( returnObj ); // root

               void *const pixMem = JS_malloc( cx, pixBytes );
               if (pixMem)
               {
                  Scale16::rotate90( (unsigned short *)pixMem, pixMap, width,height);

                  JSString *sScaleMap = JS_NewString( cx, (char *)pixMem, pixBytes );
                  if( sScaleMap )
                  {
                     JS_DefineProperty( cx, returnObj, "pixBuf", STRING_TO_JSVAL( sScaleMap ), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
                     JS_DefineProperty( cx, returnObj, "width",    INT_TO_JSVAL(height), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
                     JS_DefineProperty( cx, returnObj, "height",   INT_TO_JSVAL(width), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
                  }
                  else
                     JS_ReportError( cx, "Error building Scale map string" );
               }
               else
                  JS_ReportError( cx, "Error allocating Scale map string" );
            }
            else
               JS_ReportError( cx, "allocating array" );
         }
         else
            JS_ReportError( cx, "Invalid width or height" );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   return JS_TRUE ;
}

static JSBool
jsGetPixel( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( ( 2 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] ) )
   {
      unsigned x = JSVAL_TO_INT( argv[0] );
      unsigned y = JSVAL_TO_INT( argv[1] );
      
      jsval widthVal, heightVal, dataVal ;
      if( JS_GetProperty( cx, obj, "width", &widthVal )
          &&
          JS_GetProperty( cx, obj, "height", &heightVal )
          &&
          JS_GetProperty( cx, obj, "pixBuf", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal ) )
      {
         int const width  = JSVAL_TO_INT( widthVal );
         int const height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         unsigned const pixBytes = width * height * sizeof( pixMap[0] );
         if( JS_GetStringLength( pixStr ) == pixBytes )
         {
            if( ( x < (unsigned)width ) && ( y < (unsigned)height ) )
            {
               unsigned short rgb16 = pixMap[(y*width)+x];
               unsigned red   = fbDevice_t::getRed( rgb16 );
               unsigned green = fbDevice_t::getGreen( rgb16 );
               unsigned blue  = fbDevice_t::getBlue( rgb16 );
               unsigned long rgb = (red<<16) | (green<<8) | blue ;
               *rval = INT_TO_JSVAL( rgb );
            }
            else
               JS_ReportError( cx, "Invalid position: %u x %u, image dim %u x %u\n", x, y, width, height );
         }
         else
            JS_ReportError( cx, "Invalid width or height" );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   else
      JS_ReportError( cx, "Usage: image.getPixel( x, y );" );

   return JS_TRUE ;
}

/*
 * sets alpha for all pixels in the rect defined by x1, y1 and x2, y2
 * not including the pixels in column x2 and row y2.
 */
static JSBool
setAlpha(JSContext *cx, JSObject *obj,  unsigned short x1, unsigned short y1,
         unsigned short x2, unsigned short y2, unsigned char alp, jsval *rval)
{
   unsigned char *alpha = 0;
   jsval alphaVal, widthVal, heightVal;
   int width, height;
   JSString *sAlpha;

   *rval = JSVAL_FALSE;

   if(JS_GetProperty(cx, obj, "width", &widthVal)
      &&
      JS_GetProperty(cx, obj, "height", &heightVal))
   {
      width = JSVAL_TO_INT(widthVal);
      height = JSVAL_TO_INT(heightVal);
   }
   else
      return JSVAL_FALSE;

   if(JS_GetProperty(cx, obj, "alpha", &alphaVal)
      &&
      JSVAL_IS_STRING(alphaVal)
      &&
      (0 != (sAlpha = JSVAL_TO_STRING(alphaVal))))
   {
      alpha = (unsigned char *)JS_GetStringBytes( sAlpha );
   } // have alpha channel... use it
   else
      return JSVAL_FALSE;

   /* this is possible if passed value is a -ve value */
   if(x1 > width)
      x1 = width;
   if(x2 > width)
      x2 = width;
   /* this is possible if passed value is a -ve value */
   if(y1 > width)
      y1 = width;
   if(y2 > width)
      y2 = width;


   for(int ridx = y1; ridx < y2; ridx++) {
      unsigned char *row_data = alpha + (ridx * width);
      for(int cidx = x1; cidx < x2; cidx++) {
         row_data[cidx] = alp;
      }
   }
   return JS_TRUE ;
}

static JSBool
jsSetPixel( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
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
      unsigned x = JSVAL_TO_INT( argv[0] );
      unsigned y = JSVAL_TO_INT( argv[1] );
      unsigned rgb16 = fbDevice_t::get16( JSVAL_TO_INT( argv[2] ) );
      
      jsval widthVal, heightVal, dataVal ;
      if( JS_GetProperty( cx, obj, "width", &widthVal )
          &&
          JS_GetProperty( cx, obj, "height", &heightVal )
          &&
          JS_GetProperty( cx, obj, "pixBuf", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal ) )
      {
         int const width  = JSVAL_TO_INT( widthVal );
         int const height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         unsigned const pixBytes = width * height * sizeof( pixMap[0] );
         if( JS_GetStringLength( pixStr ) == pixBytes )
         {
            if( ( x < (unsigned)width ) && ( y < (unsigned)height ) )
            {
               pixMap[(y*width)+x] = rgb16 ;
               setAlpha(cx, obj, x, y, x+1, y+1, 255, rval); //make the drawn rect opaque
            }
            else
               JS_ReportError( cx, "Invalid position: %u x %u, image dim %u x %u\n", x, y, width, height );
         }
         else
            JS_ReportError( cx, "Invalid width or height" );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   else
      JS_ReportError( cx, "Usage: image.setPixel( x, y, rgb );" );

   return JS_TRUE ;
}

static JSBool
jsImageRect( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
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
      unsigned long const color = ( 5 <= argc ) ? JSVAL_TO_INT( argv[4] ) : 0 ;
      unsigned char const red   = (unsigned char)( color >> 16 );
      unsigned char const green = (unsigned char)( color >> 8 );
      unsigned char const blue  = (unsigned char)( color );
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
         int const width  = JSVAL_TO_INT( widthVal );
         int const height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         unsigned const pixBytes = width * height * sizeof( pixMap[0] );
         if( JS_GetStringLength( pixStr ) == pixBytes )
         {
            getFB().rect( x1, y1, x2, y2, red, green, blue, pixMap, width, height );
            setAlpha(cx, obj, x1, y1, x2, y2, 255, rval); //make the drawn rect opaque
         }
         else
            JS_ReportError( cx, "Invalid width or height" );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   else
      JS_ReportError( cx, "Usage: image.rect( x1, y1, x2, y2 [,color=0] );" );

   return JS_TRUE ;
}

static JSBool
jsImageResetAlpha( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   unsigned char alp = 0;
   unsigned short startx = 0, starty = 0, endx = 0, endy = 0;

   *rval = JSVAL_FALSE ;
   for(uint idx = 0; idx < argc; idx++)
      if(!JSVAL_IS_INT(argv[idx]))
         return JSVAL_FALSE;

   switch(argc)
   {
      case 0:
         alp = 0;
         break;
      case 1:
         alp = (unsigned char) JSVAL_TO_INT(argv[0]);
         break;
      case 4:
         startx = (unsigned short)JSVAL_TO_INT(argv[0]);
         starty = (unsigned short)JSVAL_TO_INT(argv[1]);
         endx = (unsigned short)JSVAL_TO_INT(argv[2]);
         endy = (unsigned short)JSVAL_TO_INT(argv[3]);
         break;
      case 5:
         alp = (unsigned char) JSVAL_TO_INT(argv[0]);
         startx = (unsigned short)JSVAL_TO_INT(argv[1]);
         starty = (unsigned short)JSVAL_TO_INT(argv[2]);
         endx = (unsigned short)JSVAL_TO_INT(argv[3]);
         endy = (unsigned short)JSVAL_TO_INT(argv[4]);
         break;
   }

   return setAlpha(cx, obj, startx, starty, endx, endy, alp, rval);
}

static JSBool
jsImageLine( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
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
      unsigned long const color = ( 6 <= argc ) ? JSVAL_TO_INT( argv[5] ) : 0 ;
      unsigned char const red   = (unsigned char)( color >> 16 );
      unsigned char const green = (unsigned char)( color >> 8 );
      unsigned char const blue  = (unsigned char)( color );
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
         int const width  = JSVAL_TO_INT( widthVal );
         int const height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         unsigned const pixBytes = width * height * sizeof( pixMap[0] );

         if( JS_GetStringLength( pixStr ) == pixBytes )
         {
            getFB().line( x1, y1, x2, y2, penWidth, red, green, blue, pixMap, width, height );
            setAlpha(cx, obj, x1, y1, x2+penWidth, y2+penWidth, 255, rval);
         }
         else
            JS_ReportError( cx, "Invalid width or height" );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   else
      JS_ReportError( cx, "Usage: image.line( x1, y1, x2, y2 [penWidth=1,[,color=0]] );" );

   return JS_TRUE ;
}

static JSBool
jsImageBox( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
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
      unsigned long const color = ( 6 <= argc ) ? JSVAL_TO_INT( argv[5] ) : 0 ;
      unsigned char const red   = (unsigned char)( color >> 16 );
      unsigned char const green = (unsigned char)( color >> 8 );
      unsigned char const blue  = (unsigned char)( color );
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
         int const width  = JSVAL_TO_INT( widthVal );
         int const height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         unsigned const pixBytes = width * height * sizeof( pixMap[0] );

         if( JS_GetStringLength( pixStr ) == pixBytes )
         {
            getFB().box( x1, y1, x2, y2, penWidth, red, green, blue, pixMap, width, height );
            setAlpha(cx, obj, x1, y1, x2, y1+penWidth, 255, rval);
            setAlpha(cx, obj, x1, y1, x1+penWidth, y2, 255, rval);
            setAlpha(cx, obj, x2, y1, x2+penWidth, y2+penWidth, 255, rval);
            setAlpha(cx, obj, x1, y2, x2+penWidth, y2+penWidth, 255, rval);
         }
         else
            JS_ReportError( cx, "Invalid width or height" );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   else
      JS_ReportError( cx, "Usage: image.line( x1, y1, x2, y2 [penWidth=1,[,color=0]] );" );

   return JS_TRUE ;
}

static JSFunctionSpec imageMethods_[] = {
    {"draw",         jsImageDraw,           3 },
    {"dither",       jsImageDither,         3 },
    {"scale",        jsImageScale,          6,0,0 },
    {"rotate90",     jsImageRotate90,       0,0,0 },
    {"getPixel",     jsGetPixel,            2,0,0 },
    {"setPixel",     jsSetPixel,            3,0,0 },
    {"rect",         jsImageRect,           6,0,0 },
    {"line",         jsImageLine,           6,0,0 },
    {"box",          jsImageBox,            6,0,0 },
    {"setAlpha",     jsImageResetAlpha,     6,0,0 },
    {0}
};

enum jsImage_tinyId {
   IMAGE_ISLOADED,
   IMAGE_WIDTH,
   IMAGE_HEIGHT,
   IMAGE_PIXBUF,
   IMAGE_ALPHA,
};

JSClass jsImageClass_ = {
  "image",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec imageProperties_[] = {
  {"isLoaded",      IMAGE_ISLOADED,  JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"width",         IMAGE_WIDTH,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"height",        IMAGE_HEIGHT,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"pixBuf",        IMAGE_PIXBUF,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"alpha",         IMAGE_ALPHA,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static void imageOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   bool            worked = false ;
   void const     *pixMap = 0 ;
   void const     *alpha = 0 ;
   unsigned short  width ;
   unsigned short  height ;
   std::string     sError ;
   char const *cData = (char const *)data ;
   if( ( 'P' == cData[1] ) && ( 'N' == cData[2] ) && ( 'G' == cData[3] ) )
   {
      worked = imagePNG( cData, size, pixMap, width, height, alpha );
      if( !worked )
         sError = "error converting PNG" ;
   }
   else if( ('\xff' == cData[0] ) && ( '\xd8' == cData[1] ) && ( '\xff' == cData[2] ) && ( '\xe0' == cData[3] ) )
   {
      worked = imageJPEG( cData, size, pixMap, width, height );
      if( !worked )
         sError = "error converting JPEG" ;
   }
   else if( 0 == memcmp( data, "GIF8", 4 ) )
   {
      worked = imageGIF( cData, size, pixMap, width, height, alpha );
      if( !worked )
         sError = "error converting GIF" ;
   }
   else
   {
      sError = "unknown image type: " ;
   }

   if( worked )
   {
      JS_DefineProperty( req.cx_, req.lhObj_, "width",
                         INT_TO_JSVAL( width ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_DefineProperty( req.cx_, req.lhObj_, "height",
                         INT_TO_JSVAL( height ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JSString *sPix = JS_NewStringCopyN( req.cx_, (char const *)pixMap, width*height*sizeof(unsigned short) );
      JS_DefineProperty( req.cx_, req.lhObj_, "pixBuf",
                         STRING_TO_JSVAL( sPix ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      if( alpha )
      {
         JSString *sAlpha = JS_NewStringCopyN( req.cx_, (char const *)alpha, width*height );
         JS_DefineProperty( req.cx_, req.lhObj_, "alpha",
                            STRING_TO_JSVAL( sAlpha ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
      }
   }
   else
   {   
   }

   if( alpha )
      delete [] (unsigned char *)alpha ;

   if( pixMap )
      delete [] (unsigned short *)pixMap ;
}

static JSBool image( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   static char const usage[] = {
      "Usage : new image( { url:\"something\" | width:int, height:int [,bgColor:0xRRGGBB] }"
#if CONFIG_JSCAIRO == 1
                          "| cairo_surface );" 
#endif			  
   };

   *rval = JSVAL_FALSE ;
   JSObject *rhObj ;
   if( ( 1 == argc ) 
       && 
       JSVAL_IS_OBJECT( argv[0] ) 
       &&
       ( 0 != ( rhObj = JSVAL_TO_OBJECT( argv[0] ) ) ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsImageClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
#if CONFIG_JSCAIRO == 1
         if( JS_InstanceOf( cx, rhObj, &jsCairoSurfaceClass_, NULL ) )
         {
            cairo_surface_t *surf = surfObject( cx, rhObj );
            if( surf )
            {
               printf( "create image here\n" );
               cairo_surface_flush(surf);
               unsigned char const *const pixels = (unsigned char *)
                                                   cairo_surface_get_user_data( surf, &cairoPixelKey_ );
               if( pixels )
               {
                  // TBD: How to get format
               }
               else
                  JS_ReportError(cx, "invalid pixel data\n" );               
            }
            else
               JS_ReportError( cx, usage );
         }
         else
#endif	 
         {
            jsval urlv ;
            jsval widthv ;
            jsval heightv ;
            if( JS_GetProperty( cx, rhObj, "url", &urlv ) 
                &&
                JSVAL_IS_STRING( urlv ) )
            {
               if( queueCurlRequest( thisObj, argv[0], cx, imageOnComplete ) )
               {
               }
               else
               {
                  JS_ReportError( cx, "Error queueing curlRequest" );
               }
            }
            else if( JS_GetProperty( cx, rhObj, "width", &widthv ) 
                     &&
                     JSVAL_IS_INT( widthv )
                     &&
                     JS_GetProperty( cx, rhObj, "height", &heightv )
                     &&
                     JSVAL_IS_INT( heightv ) )
            {
               unsigned short const w = JSVAL_TO_INT( widthv );
               unsigned short const h = JSVAL_TO_INT( heightv );
               unsigned long rgb32 = 0xFFFFFF ; // default white
               jsval colorv ;
               if( JS_GetProperty( cx, rhObj, "bgColor", &colorv ) 
                   &&
                   JSVAL_IS_INT( colorv ) )
               {
                  rgb32 = JSVAL_TO_INT( colorv );
               }
               JS_DefineProperty( cx, thisObj, "width",
                                  INT_TO_JSVAL( w ),
                                  0, 0, 
                                  JSPROP_ENUMERATE
                                  |JSPROP_PERMANENT
                                  |JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "height",
                                  INT_TO_JSVAL( h ),
                                  0, 0, 
                                  JSPROP_ENUMERATE
                                  |JSPROP_PERMANENT
                                  |JSPROP_READONLY );
               unsigned const pixBytes = w*h*sizeof(unsigned short);
               unsigned short *pixMap = (unsigned short *)JS_malloc( cx, pixBytes );
               if( pixMap )
               {
                  unsigned short *nextOut = pixMap ;
                  for( unsigned r = 0 ; r < h ; r++ )
                     for( unsigned c = 0 ; c < w ; c++ )
                        *nextOut++ = rgb32 ;
                  JSString *sPix = JS_NewString( cx, (char *)pixMap, pixBytes );
                  JS_DefineProperty( cx, thisObj, "pixBuf",
                                     STRING_TO_JSVAL( sPix ),
                                     0, 0, 
                                     JSPROP_ENUMERATE
                                     |JSPROP_PERMANENT
                                     |JSPROP_READONLY );
                  JS_DefineProperty( cx, thisObj, "isLoaded",
                                     JSVAL_TRUE,
                                     0, 0, 
                                     JSPROP_ENUMERATE
                                     |JSPROP_PERMANENT
                                     |JSPROP_READONLY );
   
               }
               else
                  JS_ReportError( cx, "Out of memory allocating image" );
            }
            else
               JS_ReportError( cx, usage );
         } // not cairo
      }
      else
         JS_ReportError( cx, "Error allocating image" );
   } // one parameter: object
   else
      JS_ReportError( cx, usage );
      
   return JS_TRUE ;

}

static JSBool
jsImageInfo( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   JSString *str ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] )
       &&
       ( 0 != ( str = JS_ValueToString(cx, argv[0]) ) ) )
   {
      imageInfo_t info ;
      if( getImageInfo( JS_GetStringBytes( str ), 
                        JS_GetStringLength( str ), 
                        info ) )
      {
         JSObject *infoObj = JS_NewObject( cx, &global_class, NULL, NULL);
         if( infoObj ){
            *rval = OBJECT_TO_JSVAL( infoObj ); // root
            JS_DefineProperty( cx, infoObj, "width", INT_TO_JSVAL(info.width_), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
            JS_DefineProperty( cx, infoObj, "height", INT_TO_JSVAL(info.height_), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
         }
      }
      else
         JS_ReportError( cx, "Invalid or unsupported image\n" );
   }
   else
      JS_ReportError( cx, "Usage: imageInfo( imgDataString )\n" );

   return JS_TRUE ;
}

static bool imagePsOutput( char const *outData,
                           unsigned    outLength,
                           void       *opaque )
{
   jsval *rval = (jsval *)opaque ;
   if( JSVAL_IS_STRING( *rval ) ){
      JSString *oldStr = JSVAL_TO_STRING( *rval );
      unsigned oldLen = JS_GetStringLength( oldStr );
      unsigned newLen = oldLen + outLength ;
      char *sData = (char *)JS_malloc( execContext_, newLen );
      memcpy( sData, JS_GetStringBytes(oldStr), oldLen );
      memcpy( sData+oldLen, outData, outLength );
      JSString *sOut = JS_NewString( execContext_, sData, newLen );
      *rval = STRING_TO_JSVAL( sOut );
   }
   else {
      JSString *sOut = JS_NewStringCopyN( execContext_, outData, outLength );
      *rval = STRING_TO_JSVAL( sOut );
   }
   return true ;
}

static JSBool
jsImageToPS( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   static char const usage[] = {
      "Usage: imageToPS( { image: data, x:0, y:0, w:10, h:10 } );\n" 
   };

   *rval = JSVAL_FALSE ;
   JSObject *paramObj ;
   if( ( 1 == argc ) 
       && 
       JSVAL_IS_OBJECT(argv[0])
       &&
       ( 0 != ( paramObj = JSVAL_TO_OBJECT(argv[0]) ) ) )
   {
      JSString *sImageData ;
      unsigned x, y ;
      jsval val ;
      if( JS_GetProperty( cx, paramObj, "x", &val ) 
          && 
          ( x = JSVAL_TO_INT(val), JSVAL_IS_INT( val ) )
          &&
          JS_GetProperty( cx, paramObj, "y", &val ) 
          && 
          ( y = JSVAL_TO_INT(val), JSVAL_IS_INT( val ) )
          &&
          JS_GetProperty( cx, paramObj, "image", &val ) 
          && 
          JSVAL_IS_STRING( val )
          &&
          ( 0 != ( sImageData = JSVAL_TO_STRING( val ) ) ) )
      {
         char const *const cImage = JS_GetStringBytes( sImageData );
         unsigned const    imageLen = JS_GetStringLength( sImageData );
         imageInfo_t imInfo ;
         if( getImageInfo( cImage, imageLen, imInfo ) ){
            unsigned w = imInfo.width_ ;
            unsigned h = imInfo.height_ ;
            if( JS_GetProperty( cx, paramObj, "w", &val ) && JSVAL_IS_INT( val ) )
               w = JSVAL_TO_INT( val );
            if( JS_GetProperty( cx, paramObj, "h", &val ) && JSVAL_IS_INT( val ) )
               h = JSVAL_TO_INT( val );

            rectangle_t r ;
            r.xLeft_ = x ;
            r.yTop_ = y ;
            r.width_ = w ;
            r.height_ = h ;

            if( imageToPS( cImage, imageLen,
                           r, imagePsOutput, rval ) ){
            }
            else
               JS_ReportError( cx, "imageToPS: write cancelled\n" );
         }
         else
            JS_ReportError( cx, "imageToPS: Invalid or unsupported image\n" );
      }
      else
         JS_ReportError( cx, usage );
   }
   else
      JS_ReportError( cx, usage );
   
   return JS_TRUE ;
}

static JSFunctionSpec functions_[] = {
    {"imageInfo",    jsImageInfo,        0},
    {"imageToPS",    jsImageToPS,        0},
    {0}
};


bool initJSImage( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsImageClass_,
                                  image, 1,
                                  imageProperties_, 
                                  imageMethods_,
                                  0, 0 );
   if( rval )
   {
      if( JS_DefineFunctions( cx, glob, functions_ ) )
      {
         return true ;
      }
   }
   return false ;
}

