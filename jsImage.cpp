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
 * Revision 1.13  2002-11-20 00:39:02  ericn
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
#include "curlThread.h"
#include "jsGlobals.h"

static JSBool
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
          JS_GetProperty( cx, obj, "pixBuf", &dataVal ) )
      {
         int width  = JSVAL_TO_INT( widthVal ); 
         int height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short const *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );

         fbDevice_t &fb = getFB();
         if( ( 0 < width ) && ( 0 < height ) && ( xPos < fb.getWidth() ) && ( yPos < fb.getHeight() ) )
         {
            int const left = xPos ;

            for( unsigned y = 0 ; y < height ; y++, yPos++ )
            {
               if( 0 <= yPos )
               {
                  if( yPos < fb.getHeight() )
                  {
                     unsigned short *pix = fb.getRow( yPos ) + left ;
                     xPos = left ;
                     for( unsigned x = 0 ; x < width ; x++, xPos++ )
                     {
                        if( 0 <= xPos )
                        {
                           if( xPos < fb.getWidth() )
                           {
                              *pix++ = pixMap[y*width+x];
                           }
                           else
                              break; // only going further off the screen
                        }
                        else
                           pix++ ;
                     }
                  }
                  else
                     break; // only going further off the screen
               }
            }
         }
//         JS_ReportError( cx, "w:%d, h:%d", width, height );
      }

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

static JSFunctionSpec imageMethods_[] = {
    {"draw",         jsImageDraw,           3 },
    {0}
};

enum jsImage_tinyId {
   IMAGE_ISLOADED,
   IMAGE_WIDTH, 
   IMAGE_HEIGHT, 
   IMAGE_PIXBUF,
};

JSClass jsImageClass_ = {
  "image",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec imageProperties_[] = {
  {"isLoaded",      IMAGE_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"width",         IMAGE_WIDTH,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"height",        IMAGE_HEIGHT,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"pixBuf",        IMAGE_PIXBUF,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static void imageOnComplete( jsCurlRequest_t &req, curlFile_t const &f )
{
   char const *cMime = f.getMimeType();
   char const *cData = (char const *)f.getData();
   unsigned    size  = f.getSize();

   bool            worked = false ;
   void const     *pixMap = 0 ;
   unsigned short  width ;
   unsigned short  height ;
   std::string     sError ;
   if( ( 0 == strcmp( cMime, "image/png" ) )
       ||
       ( 0 == strcmp( cMime, "image/x-png" ) ) )
   {
      worked = imagePNG( cData, size, pixMap, width, height );
      if( !worked )
         sError = "error converting PNG" ;
   }
   else if( 0 == strcmp( cMime, "image/jpeg" ) )
   {
      worked = imageJPEG( cData, size, pixMap, width, height );
      if( !worked )
         sError = "error converting PNG" ;
   }
   else if( 0 == strcmp( cMime, "image/gif" ) )
   {
      worked = imageGIF( cData, size, pixMap, width, height );
      if( !worked )
         sError = "error converting PNG" ;
   }
   else
   {
      sError = "unknown mime type: " ;
      sError += cMime ;
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
      jsCurlOnComplete( req, f );
   }
   else
   {   
      JSString *errorStr = JS_NewStringCopyN( req.cx_, sError.c_str(), sError.size() );
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "loadErrorMsg",
                         STRING_TO_JSVAL( errorStr ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      jsCurlOnError( req, f );
   }

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
      JSObject *thisObj = js_NewObject( cx, &jsImageClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         JS_DefineProperty( cx, thisObj, "isLoaded",
                            JSVAL_FALSE,
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JS_DefineProperty( cx, thisObj, "initializer",
                            argv[0],
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JSObject *const rhObj = JSVAL_TO_OBJECT( argv[0] );
         
         jsCurlRequest_t request ;
         request.lhObj_      = thisObj ;
         request.rhObj_      = rhObj ;
         request.cx_         = cx ;
         request.onComplete  = imageOnComplete ;
         request.onError     = jsCurlOnError ;
         
         if( queueCurlRequest( request, 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) ) )
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

