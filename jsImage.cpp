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
 * Revision 1.28  2004-05-08 14:23:02  ericn
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
            else
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
                     printf( "draw image to image\n" );
                  }
                  else
                     JS_ReportError( cx, "Invalid destination pixels" );
               }
               else
                  JS_ReportError( cx, "Invalid destination image" );
            }
            
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
      JS_ReportError( cx, "Usage: image().draw( x, y )" );
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
               for( unsigned y = 0 ; y < height ; y++ )
               {
                  for( unsigned x = 0 ; x < width ; x++ )
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
                  Scale16::scale( (unsigned short *)pixMem, destWidth, destHeight,pixMap, width,height, srcLeft,srcTop,srcWidth,srcHeight);

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
            if( ( x < width ) && ( y < height ) )
            {
               pixMap[(y*width)+x] = rgb16 ;
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
      JS_ReportError( cx, "Usage: screen.getPixel( x, y );" );

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
   return JS_TRUE ;
}

static JSFunctionSpec imageMethods_[] = {
    {"draw",         jsImageDraw,           3 },
    {"dither",       jsImageDither,         3 },
    {"scale",        jsImageScale,          6,0,0 },
    {"rotate90",     jsImageRotate90,       0,0,0 },
    {"setPixel",     jsSetPixel,            3,0,0 },
    {"rect",         jsImageRect,           6,0,0 },
    {"line",         jsImageLine,           6,0,0 },
    {"box",          jsImageBox,            6,0,0 },
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
      "Usage : new image( { url:\"something\" | width:int, height:int [,bgColor:0xRRGGBB] } );"
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
               fbDevice_t &fb = getFB();
               unsigned short const rgb16 = fb.get16( rgb32 );
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
      }
      else
         JS_ReportError( cx, "Error allocating image" );
   }
   else
      JS_ReportError( cx, usage );
      
   return JS_TRUE ;

}

bool initJSImage( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsImageClass_,
                                  image, 1,
                                  imageProperties_, 
                                  imageMethods_,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      return false ;

}

