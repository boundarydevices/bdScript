/*
 * Module jsScreen.cpp
 *
 * This module defines the screen-handling routines for
 * Javascript
 *
 *
 * Change History : 
 *
 * $Log: jsScreen.cpp,v $
 * Revision 1.6  2002-11-21 14:05:19  ericn
 * -added invertRect() method
 *
 * Revision 1.5  2002/11/08 13:58:16  ericn
 * -modified to handle negative screen positions
 *
 * Revision 1.4  2002/11/02 18:36:54  ericn
 * -added getPixel, setPixel, getRect
 *
 * Revision 1.3  2002/10/31 02:08:10  ericn
 * -made screen object, got rid of bare clearScreen method
 *
 * Revision 1.2  2002/10/20 16:30:49  ericn
 * -modified to allow clear to specified color
 *
 * Revision 1.1  2002/10/18 01:18:25  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsScreen.h"
#include "fbDev.h"
#include <string.h>
#include "js/jsstddef.h"
#include "js/jscntxt.h"
#include "js/jsapi.h"
#include "js/jslock.h"
#include "jsImage.h"

static JSBool
jsClearScreen( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   fbDevice_t &fb = getFB();
   
   if( 0 == argc )
   {
      memset( fb.getMem(), 0, fb.getMemSize() );
   } // clear to black
   else
   {
      unsigned long const rgb = (unsigned long)JSVAL_TO_INT( argv[0] );
      unsigned char red   = (rgb>>16) & 0xFF ;
      unsigned char green = (rgb>>8) & 0xFF ;
      unsigned char blue  = rgb & 0xFF ;
      unsigned short color16 = fb.get16( red, green, blue );
      unsigned short *start = (unsigned short *)fb.getMem();
      unsigned short *end   = start + fb.getHeight() * fb.getWidth();
      while( start < end )
         *start++ = color16 ;
   } // clear to specified color
         
   *rval = JSVAL_TRUE ;
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
      fbDevice_t &fb = getFB();
      
      if( ( x < fb.getWidth() ) && ( y < fb.getHeight() ) )
      {
         unsigned short const rgb16 = fb.getPixel( x, y );
         unsigned red   = fb.getRed( rgb16 );
         unsigned green = fb.getGreen( rgb16 );
         unsigned blue  = fb.getBlue( rgb16 );
         unsigned rgb = ( red << 16 ) | ( green << 8 ) | blue ;
         *rval = INT_TO_JSVAL( rgb );
      }
      else
         JS_ReportError( cx, "Invalid screen position" );
   }
   else
      JS_ReportError( cx, "Usage: screen.getPixel( x, y );" );

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
      unsigned rgb = JSVAL_TO_INT( argv[2] );

      fbDevice_t &fb = getFB();
      
      if( ( x < fb.getWidth() ) && ( y < fb.getHeight() ) )
      {
         unsigned char red   = rgb >> 16 ;
         unsigned char green = ( rgb >> 8 ) & 0xFF ;
         unsigned char blue  = rgb & 0xFF ;

         fb.getPixel( x, y ) = fb.get16( red, green, blue );
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "Invalid screen position" );
   }
   else
      JS_ReportError( cx, "Usage: screen.getPixel( x, y );" );

   return JS_TRUE ;
}

static JSBool
jsGetRect( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 4 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] )
       &&
       JSVAL_IS_INT( argv[3] ) )
   {
      JSObject *rImage = JS_NewObject( cx, &jsImageClass_, 0, 0 );
      if( rImage )
      {
         int width = JSVAL_TO_INT( argv[2] );
         int height = JSVAL_TO_INT( argv[3] );
         if( ( 0 < width ) && ( 0 < height ) )
         {
            *rval = OBJECT_TO_JSVAL( rImage );
            JS_DefineProperty( cx, rImage, "width",
                               argv[2],
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, rImage, "height", 
                               argv[3],
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            unsigned const pixBytes = width*height*sizeof(unsigned short);
            unsigned short * const pixels = (unsigned short *)JS_malloc( cx, pixBytes );
            if( pixels )
            {
               int const startX = JSVAL_TO_INT( argv[0] );
               int const startY = JSVAL_TO_INT( argv[1] );

               fbDevice_t &fb = getFB();

               for( unsigned y = 0 ; y < height ; y++ )
               {
                  int const screenY = startY + y ;
                  if( ( 0 <= screenY ) 
                      &&
                      ( screenY < fb.getHeight() ) )
                  {
                     for( unsigned x = 0 ; x < width ; x++ )
                     {
                        int const screenX = x + startX ;
                        if( ( 0 < screenX ) && ( screenX < fb.getWidth() ) )
                        {
                           pixels[(y*width)+x] = fb.getPixel( screenX, screenY );
                        }
                        else
                           pixels[(y*width)+x] = 0 ;
                     } // for each column
                  }
                  else
                     memset( pixels+(y*width), 0, width*sizeof(pixels[0]));
               } // for each row requested

               JSString *sPix = JS_NewString( cx, (char *)pixels, pixBytes );
               if( sPix )
               {
                  JS_DefineProperty( cx, rImage, "pixBuf", 
                                     STRING_TO_JSVAL( sPix ),
                                     0, 0, 
                                     JSPROP_ENUMERATE
                                     |JSPROP_PERMANENT
                                     |JSPROP_READONLY );
               }
               else
               {
                  JS_ReportError( cx, "Error allocating pixStr" );
                  JS_free( cx, pixels );
               }
            }
            else
               JS_ReportError( cx, "Error allocating pixBuf" );
         }
         else
            JS_ReportError( cx, "Invalid width or height" );
      }
      else
         JS_ReportError( cx, "Error allocating image rect" );
   }
   else
      JS_ReportError( cx, "Usage: screen.getRect( x, y, width, height );" );

   return JS_TRUE ;
}

static JSBool
jsInvertRect( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 4 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] )
       &&
       JSVAL_IS_INT( argv[3] ) )
   {
      int width = JSVAL_TO_INT( argv[2] );
      int height = JSVAL_TO_INT( argv[3] );
      if( ( 0 < width ) && ( 0 < height ) )
      {
         int const startX = JSVAL_TO_INT( argv[0] );
         int const startY = JSVAL_TO_INT( argv[1] );

         fbDevice_t &fb = getFB();

         for( unsigned y = 0 ; y < height ; y++ )
         {
            int const screenY = startY + y ;
            if( ( 0 <= screenY ) 
                &&
                ( screenY < fb.getHeight() ) )
            {
               for( unsigned x = 0 ; x < width ; x++ )
               {
                  int const screenX = x + startX ;
                  if( ( 0 < screenX ) && ( screenX < fb.getWidth() ) )
                  {
                     fb.getPixel( screenX, screenY ) = ~fb.getPixel( screenX, screenY );
                  } // visible column
               } // for each column
            } // visible row
         } // for each row requested
      }
      else
         JS_ReportError( cx, "Invalid width or height" );
   }
   else
      JS_ReportError( cx, "Usage: screen.invertRect( x, y, width, height );" );

   return JS_TRUE ;
}

enum jsScreen_tinyId {
   SCREEN_WIDTH, 
   SCREEN_HEIGHT, 
   SCREEN_PIXBUF,
};

extern JSClass jsScreenClass_ ;

JSClass jsScreenClass_ = {
  "Screen",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec screenProperties_[] = {
  {"width",         SCREEN_WIDTH,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"height",        SCREEN_HEIGHT,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};


//
// constructor for the screen object
//
static JSBool jsScreen( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   obj = js_NewObject( cx, &jsScreenClass_, NULL, NULL );

   if( obj )
   {
      fbDevice_t &fb = getFB();
      
      JS_DefineProperty( cx, obj, "width",
                         INT_TO_JSVAL( fb.getWidth() ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_DefineProperty( cx, obj, "height", 
                         INT_TO_JSVAL( fb.getHeight() ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      *rval = OBJECT_TO_JSVAL(obj);
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

static JSFunctionSpec screen_methods[] = {
   { "clear",        jsClearScreen,      0,0,0 },
   { "getPixel",     jsGetPixel,         0,0,0 },
   { "setPixel",     jsSetPixel,         0,0,0 },
   { "getRect",      jsGetRect,          0,0,0 },
   { "invertRect",   jsInvertRect,       0,0,0 },
   { 0 }
};

bool initJSScreen( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsScreenClass_,
                                  jsScreen, 1,
                                  screenProperties_, 
                                  screen_methods,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      return false ;
}

