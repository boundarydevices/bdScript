/*
 * Module jsCBM.cpp
 *
 * This module defines the initialization and Javascript interface
 * routines for the CBM printer.
 *
 *
 * Change History : 
 *
 * $Log: jsCBM.cpp,v $
 * Revision 1.2  2003-05-10 03:18:54  ericn
 * -removed redundant test, changed CBM print command
 *
 * Revision 1.1  2003/05/09 04:28:12  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsCBM.h"
#include <string.h>
#include "js/jsstddef.h"
#include "js/jscntxt.h"
#include "js/jsapi.h"
#include "js/jslock.h"
#include "jsAlphaMap.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/lp.h>
#include <sys/ioctl.h>
#include <termios.h>

enum jsCBM_tinyId {
   CBM_WIDTH, 
   CBM_HEIGHT, 
   CBM_PIXBUF,
};

JSClass jsCBMClass_ = {
  "CBM",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec cbmProperties_[] = {
  {0,0,0}
};

static JSBool
jsCBMPrint( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
fprintf( stderr, "--> Printing something\n" );
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
         unsigned const bmHeight   = JSVAL_TO_INT( vHeight );
         if( JS_GetStringLength( sPixMap ) == bmWidth*bmHeight )
         {
            unsigned char const * const alphaData = (unsigned char const *)JS_GetStringBytes( sPixMap );
            int const fd = open( "/dev/usb/lp0", O_WRONLY );
            if( 0 <= fd )
            {
               printf( "printer port opened\n" );
         
               unsigned const width = bmWidth ;
               unsigned const wBits = ((width+7)/8)*8 ;
               unsigned const height = bmHeight ;
               unsigned const hBytes = (height+7)/8 ;
               unsigned const headerLen = 4 ;
               unsigned const trailerLen = 9 ;
               char * const outBuf = new char [ headerLen+trailerLen + wBits*hBytes ];
               char *nextOut = outBuf ;
         
               nextOut += sprintf( outBuf, "\x1d*%c%c", wBits/8, hBytes );
         
               for( unsigned x = 0 ; x < wBits ; x++ )
               {
                  if( x < bmWidth )
                  {
                     for( unsigned y = 0 ; y < hBytes ; y++ )
                     {
                        unsigned yPixel = 8*y ;
                        unsigned char outMask = 0 ;
                        unsigned const maxY = yPixel + 8 ;
                        for( ; yPixel < maxY ; yPixel++ )
                        {
                           outMask <<= 1 ;
                           if( ( yPixel < bmHeight ) && ( x < bmWidth ) )
                           {
                              if( 0 == alphaData[ yPixel*bmWidth+x ] )
                                 outMask |= 1 ;
                           }
                        }
                        *nextOut++ = outMask ;
                     }
                  }
                  else
                  {
                     memset( nextOut, 0, hBytes );
                     nextOut += hBytes ;
                  }
               }

               nextOut += sprintf( nextOut, "\x1d/%c\n\n\n\n", 0 );

               int const numWritten = write( fd, outBuf, nextOut-outBuf );
               printf( "wrote %d bytes\n", numWritten );
         
               write( fd, "\x1dV", 3 );

               close( fd );
               
               *rval = JSVAL_TRUE ;
            }
            else
               JS_ReportError( cx, "%m opening printer" );
            
         }
         else
            JS_ReportError( cx, "Invalid pixMap" );
      }
      else
         JS_ReportError( cx, "Error retrieving alphaMap fields" );
   }
   else
      JS_ReportError( cx, "Usage: printer.print( alphaMap )" );
   
   return JS_TRUE ;
}

static JSFunctionSpec cbm_methods[] = {
   { "print",        jsCBMPrint,      0,0,0 },
   { 0 }
};

static JSBool jsCBM( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   obj = JS_NewObject( cx, &jsCBMClass_, NULL, NULL );

   if( obj )
   {
fprintf( stderr, "--> printer object allocated\n" );
      *rval = OBJECT_TO_JSVAL(obj);
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

#include <unistd.h>

bool initJSCBM( JSContext *cx, JSObject *glob )
{
fprintf( stderr, "--> CBM...CBM...\n" );

   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsCBMClass_,
                                  jsCBM, 1,
                                  cbmProperties_, 
                                  cbm_methods,
                                  0, 0 );
   if( rval )
   {
      JSObject *obj = JS_NewObject( cx, &jsCBMClass_, NULL, NULL );
      if( obj )
      {
         if( JS_DefineProperty( cx, glob, "printer", 
                                OBJECT_TO_JSVAL( obj ),
                                0, 0, 
                                JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY ) )
         {
fprintf( stderr, "--> Defined printer object\n" );
            return true ;
         }
         else
            fprintf( stderr, "Error defining printer object\n" );
      }
      else
         fprintf( stderr, "Error allocating CBM printer" );
   }
   else
      fprintf( stderr, "Error initializing CBM printer class\n" );
   return false ;
}

