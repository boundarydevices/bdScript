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
 * Revision 1.21  2009-06-09 02:52:23  tkisky
 * fix munmap size
 *
 * Revision 1.20  2009-06-03 21:26:00  tkisky
 * -add screen.release function
 *
 * Revision 1.19  2009-02-09 18:49:55  ericn
 * handle negative coordinates in screen.rect()
 *
 * Revision 1.18  2007-10-01 20:17:02  ericn
 * -[screen] fix usage message for screen.buttonize
 *
 * Revision 1.17  2005/11/06 16:01:50  ericn
 * -KERNEL_FB, not CONFIG_BD2003
 *
 * Revision 1.16  2005/11/06 00:49:37  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.15  2004/12/05 00:55:26  tkisky
 * -more info in error message
 *
 * Revision 1.14  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.13  2003/11/30 16:45:31  ericn
 * -use prototype for global instance
 *
 * Revision 1.12  2003/02/08 14:56:27  ericn
 * -removed redundant declaration
 *
 * Revision 1.11  2002/12/27 23:30:54  ericn
 * -made global screen
 *
 * Revision 1.10  2002/12/15 20:01:16  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.9  2002/12/11 04:04:48  ericn
 * -moved buttonize code from button to fbDev
 *
 * Revision 1.8  2002/12/06 02:23:07  ericn
 * -fixed color mapping
 *
 * Revision 1.7  2002/12/04 13:13:06  ericn
 * -added rect, line, box methods
 *
 * Revision 1.6  2002/11/21 14:05:19  ericn
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
#include "zOrder.h"
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
      fb.clear(); // clear to normal background color
   } // clear to black
   else
   {
      unsigned long const rgb = (unsigned long)JSVAL_TO_INT( argv[0] );
      unsigned char red   = (rgb>>16) & 0xFF ;
      unsigned char green = (rgb>>8) & 0xFF ;
      unsigned char blue  = rgb & 0xFF ;
      fb.clear( red, green, blue );
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

         fb.setPixel( x, y, fb.get16( red, green, blue ) );
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

               for( unsigned y = 0 ; y < (unsigned)height ; y++ )
               {
                  int const screenY = startY + y ;
                  if( ( 0 <= screenY ) 
                      &&
                      ( screenY < fb.getHeight() ) )
                  {
                     for( unsigned x = 0 ; x < (unsigned)width ; x++ )
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
         int x = JSVAL_TO_INT( argv[0] );
         int y = JSVAL_TO_INT( argv[1] );
         fbDevice_t &fb = getFB();
         int endX = x + width;
         int endY = y + height;;
         if (endX > fb.getWidth())
        	 endX = fb.getWidth();
         if (endY > fb.getHeight())
        	 endY = fb.getHeight();
         if (x < 0)
        	 x = 0;
         if (y < 0)
        	 y = 0;
         int orig_x = x;
//	 printf("x=%x, y=%x, endx=%x, endY=%x 1stpix=%x\n", x, y, endX, endY, fb.getPixel(x, y));
         for (; y < endY ; y++ ) {
               for (int x = orig_x; x < endX ; x++ ) {
                     fb.setPixel(x, y, ~fb.getPixel(x, y));
               } // for each column
         } // for each row requested
      }
      else
         JS_ReportError( cx, "Invalid width or height" );
   }
   else
      JS_ReportError( cx, "Usage: screen.invertRect( x, y, width, height );" );

   return JS_TRUE ;
}

static JSBool
jsRect( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( ( 4 == argc ) || ( 5 == argc ) )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] )
       &&
       JSVAL_IS_INT( argv[3] ) )
   {
      unsigned long const color = ( 5 == argc ) ? JSVAL_TO_INT( argv[4] ) : 0 ;
      unsigned char const red   = (unsigned char)( color >> 16 );
      unsigned char const green = (unsigned char)( color >> 8 );
      unsigned char const blue  = (unsigned char)( color );
      short x1 = (short)JSVAL_TO_INT( argv[0] );	if( x1 < 0 ) x1 = 0 ;
      short y1 = (short)JSVAL_TO_INT( argv[1] );        if( y1 < 0 ) y1 = 0 ;
      short x2 = (short)JSVAL_TO_INT( argv[2] );        if( x2 < 0 ) x2 = 0 ;
      short y2 = (short)JSVAL_TO_INT( argv[3] );        if( y2 < 0 ) y2 = 0 ;
      getFB().rect( x1, y1, x2, y2, red, green, blue );
   }
   else
      JS_ReportError( cx, "Usage: screen.rect( x1, y1, x2, y2 [,color] );" );

   return JS_TRUE ;
}

static JSBool
jsLine( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 4 <= argc ) && ( 6 >= argc ) &&
       JSVAL_IS_INT( argv[0] ) &&
       JSVAL_IS_INT( argv[1] ) &&
       JSVAL_IS_INT( argv[2] ) &&
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
      getFB().line( x1, y1, x2, y2, penWidth, red, green, blue );
   } else {
      JS_ReportError( cx, "Usage: screen.line( x1, y1, x2, y2 [penWidth=1 [,color=0]] ); argc:%d,%d(%d),%d(%d),%d(%d),%d(%d)",argc,
		JSVAL_IS_INT( argv[0] ),JSVAL_TO_INT( argv[0] ),JSVAL_IS_INT( argv[1] ),JSVAL_TO_INT( argv[1] ),
		JSVAL_IS_INT( argv[2] ),JSVAL_TO_INT( argv[2] ),JSVAL_IS_INT( argv[3] ),JSVAL_TO_INT( argv[3] ) );
   }
   return JS_TRUE ;
}

static JSBool
jsBox( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 4 <= argc ) && ( 6 >= argc ) &&
       JSVAL_IS_INT( argv[0] ) &&
       JSVAL_IS_INT( argv[1] ) &&
       JSVAL_IS_INT( argv[2] ) &&
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
      getFB().box( x1, y1, x2, y2, penWidth, red, green, blue );
   }
   else
      JS_ReportError( cx, "Usage: screen.box( x1, y1, x2, y2 [penWidth=1 [,color=0]] );" );

   return JS_TRUE ;
}

#ifdef KERNEL_FB
static JSBool
jsButtonize( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 7 == argc )
       &&
       JSVAL_IS_BOOLEAN( argv[0] )
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
       JSVAL_IS_INT( argv[6] ) )
   {
      bool const pressed = JSVAL_TO_BOOLEAN( argv[0] );
      unsigned short border = (unsigned short)JSVAL_TO_INT( argv[1] );
      unsigned short x1 = (unsigned short)JSVAL_TO_INT( argv[2] );
      unsigned short y1 = (unsigned short)JSVAL_TO_INT( argv[3] );
      unsigned short x2 = (unsigned short)JSVAL_TO_INT( argv[4] );
      unsigned short y2 = (unsigned short)JSVAL_TO_INT( argv[5] );
      unsigned long const color = JSVAL_TO_INT( argv[6] );
      unsigned char const red   = (unsigned char)( color >> 16 );
      unsigned char const green = (unsigned char)( color >> 8 );
      unsigned char const blue  = (unsigned char)( color );
      getFB().buttonize( pressed, border, x1, y1, x2, y2, red, green, blue );
   }
   else
      JS_ReportError( cx, "Usage: screen.buttonize( pressed, borderWidth, x1, y1, x2, y2, bgcolor );" );

   return JS_TRUE ;
}
#endif 

enum jsScreen_tinyId {
   SCREEN_WIDTH, 
   SCREEN_HEIGHT, 
   SCREEN_PIXBUF,
};

JSClass jsScreenClass_ = {
  "Screen",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec screenProperties_[] = {
  {0,0,0}
};


static void set_width_height_properties(JSContext *cx, JSObject *obj)
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
}
//
// constructor for the screen object
//
static JSBool jsScreen( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   obj = JS_NewObject( cx, &jsScreenClass_, NULL, NULL );

   if( obj )
   {
      set_width_height_properties(cx, obj);
      *rval = OBJECT_TO_JSVAL(obj);
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

static JSBool jsReleaseFB( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   set_width_height_properties(cx, obj);
   release_ZMap();
   fbDevice_t::releaseFB();
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec screen_methods[] = {
   { "clear",        jsClearScreen,      0,0,0 },
   { "getPixel",     jsGetPixel,         0,0,0 },
   { "setPixel",     jsSetPixel,         0,0,0 },
   { "getRect",      jsGetRect,          0,0,0 },
   { "invertRect",   jsInvertRect,       0,0,0 },
   { "rect",         jsRect,             0,0,0 },
   { "line",         jsLine,             0,0,0 },
   { "box",          jsBox,              0,0,0 },
#ifdef KERNEL_FB
   { "buttonize",    jsButtonize,        0,0,0 },
#endif 
   { "release",      jsReleaseFB,        0,0,0 },
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
      JSObject *obj = JS_NewObject( cx, &jsScreenClass_, rval, glob );
      if( obj )
      {
         JS_DefineProperty( cx, glob, "screen", 
                            OBJECT_TO_JSVAL( obj ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         set_width_height_properties(cx, obj);
         return true ;
      }
   }
   return false ;
}

