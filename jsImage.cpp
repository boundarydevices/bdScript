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
 * Revision 1.26  2004-03-17 04:56:19  ericn
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
   //
   // need x and y
   //
   if( 2 == argc )
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
         int width  = JSVAL_TO_INT( widthVal ); 
         int height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short const *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         if( JS_GetStringLength( pixStr ) == width * height * sizeof( pixMap[0] ) )
         {
            fbDevice_t &fb = getFB();
   
            jsval     alphaVal ;
            JSString *sAlpha ;
            if( JS_GetProperty( cx, obj, "alpha", &alphaVal ) 
                &&
                JSVAL_IS_STRING( alphaVal )
                &&
                ( 0 != ( sAlpha = JSVAL_TO_STRING( alphaVal ) ) ) )
            {
               unsigned char const *alpha = (unsigned char const *)JS_GetStringBytes( sAlpha );
               fb.render(xPos,yPos,width,height,pixMap, alpha );
            } // have alpha channel... use it
            else
               fb.render(xPos,yPos,width,height,pixMap);
         }
         else
            JS_ReportError( cx, "Invalid pixMap\n" );

//         JS_ReportError( cx, "w:%d, h:%d", width, height );
      }
      else
         JS_ReportError( cx, "Object not initialized, can't draw\n" );

      *rval = JSVAL_TRUE ;
      return JS_TRUE ;

   } // need at least two params
   else
   {
      JS_ReportError( cx, "Usage: image().draw( x, y )" );
      JS_ReportError( cx, "#args == %d", argc );
   }

   return JS_FALSE ;
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

static JSFunctionSpec imageMethods_[] = {
    {"draw",         jsImageDraw,           3 },
    {"dither",       jsImageDither,         3 },
    {"scale",        jsImageScale,          6,0,0 },
    {"rotate90",     jsImageRotate90,       0,0,0 },
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
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) 
       && 
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsImageClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         if( queueCurlRequest( thisObj, argv[0], cx, imageOnComplete ) )
         {
         }
         else
         {
            JS_ReportError( cx, "Error queueing curlRequest" );
         }
      }
      else
         JS_ReportError( cx, "Error allocating image" );
   }
   else
      JS_ReportError( cx, "Usage : new image( { url:\"something\" } );" );
      
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

