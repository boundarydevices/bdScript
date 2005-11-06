/*
 * Module jsStar.cpp
 *
 * This module defines the Star-specific printer methods
 * as described in jsStar.h
 *
 *
 * Change History : 
 *
 * $Log: jsStar.cpp,v $
 * Revision 1.5  2005-11-06 00:49:40  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.4  2004/05/10 15:41:47  ericn
 * -split parts of initRaster to allow choice in user-space
 *
 * Revision 1.3  2004/05/08 23:55:05  ericn
 * -expanded star_methods for pageHeight() call, separate exitRaster
 *
 * Revision 1.2  2004/05/08 16:33:59  ericn
 * -high-speed
 *
 * Revision 1.1  2004/05/05 03:20:32  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "jsStar.h"
#include <string.h>
#include "js/jsstddef.h"
#include "js/jscntxt.h"
#include "js/jsapi.h"
#include "js/jslock.h"
#include "jsAlphaMap.h"
#include "dither.h"
#include <assert.h>
#include "tickMs.h"

#define DPI          203
#define PAGEHEIGHT   (DPI*2)
#define PAGEWIDTH    832
#define WIDTHBYTES   (PAGEWIDTH/8)

static char const initPrinter[] = {
   "\x1b@"              // initPrinter
};

static char const initRaster[] = {
   "\x1b*rR"            // initialize raster mode (to defaults)
   "\x1b*rA"            // enter raster mode
   "\x1b*rC"            // clear image
};

static char const draftQuality[] = {
   "\x1b*rQ0\x00"       // 0 == high speed, 1 == normal, 2 == letter quality
};

static char const normalQuality[] = {
   "\x1b*rQ0\x01"       // 0 == high speed, 1 == normal, 2 == letter quality
};

static char const letterQuality[] = {
   "\x1b*rQ0\x02"       // 0 == high speed, 1 == normal, 2 == letter quality
};

static char const setPageHeight[] = {
   "\x01b*rP"           // set page height
};

static char const leftMargin[] = {
   "\x1b*rml1\x00"
};

static char const rightMargin[] = {
   "\x1b*rmr1\x00"
};

static char const topMargin[] = {
   "\x1b*rT0\x00"
};

static char const skipLine[] = {
   "\x01b*rY1\0"
};

static char const formFeed[] = {
   "\x1b\x0C\x00" 
};

static char const exitRaster[] = {
   "\x1b*rB"            // exit raster mode
};

static JSBool
jsStarImageToString( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   JSObject *rhObj ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_OBJECT( argv[0] ) 
       &&
       ( 0 != ( rhObj = JSVAL_TO_OBJECT( argv[0] ) ) ) )
   {
      jsval     vPixMap ;
      jsval     vWidth ;
      jsval     vHeight ;
      JSString *sPixMap ;
   
      if( JS_GetProperty( cx, rhObj, "pixBuf", &vPixMap )
          &&
          JSVAL_IS_STRING( vPixMap )
          &&
          ( 0 != ( sPixMap = JSVAL_TO_STRING( vPixMap ) ) )
          &&
          JS_GetProperty( cx, rhObj, "width", &vWidth )
          &&
          JSVAL_IS_INT( vWidth )
          &&
          JS_GetProperty( cx, rhObj, "height", &vHeight )
          &&
          JSVAL_IS_INT( vHeight ) )
      {
         unsigned const bmWidth    = JSVAL_TO_INT( vWidth );
         unsigned       bmHeight   = JSVAL_TO_INT( vHeight );
         if( JS_GetStringLength( sPixMap ) == bmWidth*bmHeight*2 )
         {
            unsigned short const *pixData = (unsigned short const *)JS_GetStringBytes( sPixMap );

            unsigned const rowBytes = (bmWidth+7)/8 ;
            unsigned const imageBytes = bmHeight*(rowBytes+3);    // 'b'+n+m+pixels
            unsigned const totalBytes = + imageBytes ;

            void *const imageData = JS_malloc( cx, totalBytes );

            unsigned char *nextOut = (unsigned char *)imageData ;
/*
printf( "dither..." ); fflush( stdout );
long long const start = tickMs();
*/
            dither_t dither( pixData, bmWidth, bmHeight );
/*
long long const dEnd = tickMs();
printf( "toBits..." ); fflush( stdout );
*/
            for( unsigned y = 0 ; y < bmHeight ; y++ )
            {
               unsigned char * const startOfLine = nextOut ;
               unsigned char * lastDot = startOfLine ;

               *nextOut++ = 'b' ;
               *nextOut++ = '\0' ;
               *nextOut++ = '\0' ;
      
               unsigned char mask = '\x80' ;
               unsigned char out = 0 ;
               for( unsigned x = 0 ; x < bmWidth ; x++ )
               {
                  if( dither.isBlack( x, y ) )
                     out |= mask ;
                  
                  mask >>= 1 ;
                  if( 0 == mask )
                  {
                     mask = '\x80' ;
                     *nextOut++ = out ;
                     if( out )
                        lastDot = nextOut ;
                     out = 0 ;
                  }
               }

               if( ( '\x80' != mask ) && ( 0 != out ) )
               {
                  *nextOut++ = out ;
                  lastDot = nextOut ;
               }

               if( lastDot == startOfLine )
               {

                  memcpy( startOfLine, skipLine, sizeof( skipLine )-1 );
                  nextOut = startOfLine + sizeof( skipLine ) - 1 ;
               } // blank line
               else
               {
                  unsigned const lineBytes = lastDot - startOfLine - 3 ;
                  unsigned char const n = lineBytes / 256;
                  unsigned char const m = lineBytes & 255 ;
                  startOfLine[1] = m ;
                  startOfLine[2] = n ;
//                  nextOut = lastDot ;
               }
            }
/*
long long const rEnd = tickMs();
printf( "done\n"
        "%5llu total, %5llu dither, %5llu render\n",
        rEnd-start, dEnd-start, rEnd-dEnd ); fflush( stdout );
*/
            unsigned const outBytes = nextOut-(unsigned char *)imageData ;
            JSString *sOut = JS_NewString( cx, (char *)imageData, outBytes );
            *rval = STRING_TO_JSVAL( sOut );
// printf( "rendered %u bytes of star image data\n", outBytes );
assert( outBytes <= totalBytes );
         }
         else
            JS_ReportError( cx, "Invalid pixMap" );
      }
      else
         JS_ReportError( cx, "Error retrieving alphaMap fields" );
   }
   else
      JS_ReportError( cx, "Usage: printer.print( 'string' | img )\n" );

   return JS_TRUE ;
}

static JSBool
jsStarPageHeight( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
   {
      char cHeightSpec[80];
      int hSpecLen = sprintf( cHeightSpec, "%s%d", setPageHeight, JSVAL_TO_INT(argv[0]) ) + 1 ;
      JSString *s = JS_NewStringCopyN( cx, cHeightSpec, hSpecLen );
      *rval = STRING_TO_JSVAL( s );
   }
   else
      JS_ReportError( cx, "Usage: printer.pageHeight( #pixels )" );

   return JS_TRUE ;
}

void starPrinterFixup( JSContext *cx, 
                       JSObject  *obj )
{
   JS_DefineProperty( cx, obj, "width", 
                      INT_TO_JSVAL( PAGEWIDTH ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   JSString *s = JS_NewStringCopyN( cx, initPrinter, sizeof( initPrinter ) - 1  );
   JS_DefineProperty( cx, obj, "initPrinter", 
                      STRING_TO_JSVAL( s ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   s = JS_NewStringCopyN( cx, initRaster, sizeof( initRaster )-1 );
   JS_DefineProperty( cx, obj, "initRaster", 
                      STRING_TO_JSVAL( s ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   s = JS_NewStringCopyN( cx, formFeed, sizeof( formFeed )-1 );
   JS_DefineProperty( cx, obj, "formFeed", 
                      STRING_TO_JSVAL( s ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   s = JS_NewStringCopyN( cx, exitRaster, sizeof( exitRaster )-1 );
   JS_DefineProperty( cx, obj, "exitRaster", 
                      STRING_TO_JSVAL( s ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   s = JS_NewStringCopyN( cx, draftQuality, sizeof( draftQuality )-1 );
   JS_DefineProperty( cx, obj, "draftQuality", 
                      STRING_TO_JSVAL( s ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   s = JS_NewStringCopyN( cx, normalQuality, sizeof( normalQuality )-1 );
   JS_DefineProperty( cx, obj, "normalQuality", 
                      STRING_TO_JSVAL( s ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   s = JS_NewStringCopyN( cx, letterQuality, sizeof( letterQuality )-1 );
   JS_DefineProperty( cx, obj, "letterQuality", 
                      STRING_TO_JSVAL( s ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
}

JSFunctionSpec star_methods[3] = {
   { "imageToString",   jsStarImageToString,      0,0,0 },
   { "pageHeight",      jsStarPageHeight,         0,0,0 },
   { 0 }
};

