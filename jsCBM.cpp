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
 * Revision 1.4  2003-05-18 21:52:26  ericn
 * -added print( string ) method
 *
 * Revision 1.3  2003/05/10 19:16:43  ericn
 * -added cut command, modified to keep printer fd
 *
 * Revision 1.2  2003/05/10 03:18:54  ericn
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

static int printerFd_ = -1 ;

static JSBool
jsCBMPrint( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
fprintf( stderr, "--> Printing something\n" );
   *rval = JSVAL_FALSE ;

   JSObject *rhObj ;
   JSString *sArg ;
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

            if( 0 <= printerFd_ )
            {
               unsigned const width = bmWidth ;
               unsigned const wBits = ((width+7)/8)*8 ;
               unsigned const height = bmHeight ;
               unsigned const hBytes = (height+7)/8 ;
               unsigned const headerLen = 4 ;
               unsigned const trailerLen = 3 ;
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

               nextOut += sprintf( nextOut, "\x1d/%c", 0 );

               int const numWritten = write( printerFd_, outBuf, nextOut-outBuf );
               printf( "wrote %d bytes\n", numWritten );
               
               delete [] outBuf ;

               *rval = JSVAL_TRUE ;
            }
            else
               JS_ReportError( cx, "Invalid printer handle %d", printerFd_ );
            
         }
         else
            JS_ReportError( cx, "Invalid pixMap" );
      }
      else
         JS_ReportError( cx, "Error retrieving alphaMap fields" );
   }
   else if( ( 1 == argc )
            &&
            JSVAL_IS_STRING( argv[0] )
            &&
            ( 0 != ( sArg = JSVAL_TO_STRING( argv[0] ) ) ) )
   {
      if( 0 <= printerFd_ )
      {
         int const numWritten = write( printerFd_, 
                                       JS_GetStringBytes( sArg ), 
                                       JS_GetStringLength( sArg ) );
         printf( "wrote %d string bytes\n", numWritten );
   
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "Invalid printer handle %d", printerFd_ );
   }
   else
      JS_ReportError( cx, "Usage: printer.print( alphaMap|string )" );
   
   return JS_TRUE ;
}

static JSBool
jsCBMCut( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
fprintf( stderr, "--> CUT\n" );
   *rval = JSVAL_FALSE ;

   if( 0 <= printerFd_ )
   {
      printf( "printer port opened\n" );
         
      write( printerFd_, "\n\n\n\n\x1dV", 7 );
      
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   
   return JS_TRUE ;
}

static JSFunctionSpec cbm_methods[] = {
   { "print",        jsCBMPrint,      0,0,0 },
   { "cut",          jsCBMCut,        0,0,0 },
   { 0 }
};

static JSBool jsCBM( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   obj = JS_NewObject( cx, &jsCBMClass_, NULL, NULL );

   if( obj )
   {
      *rval = OBJECT_TO_JSVAL(obj);
   }
   else
      JS_ReportError( cx, "Error allocating printer" );

   return JS_TRUE;
}

#include <unistd.h>

bool initJSCBM( JSContext *cx, JSObject *glob )
{
fprintf( stderr, "--> CBM...CBM...\n" );

   int const fd = open( "/dev/usb/lp0", O_WRONLY );
   if( 0 <= fd )
   {
fprintf( stderr, "printer port opened: handle %d\n", fd );
      printerFd_ = fd ;
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
   }
   else
      JS_ReportError( cx, "Error %m opening printer handle" );

   return false ;
}

bool closeCBM( JSContext *cx, JSObject *glob )
{
JS_ReportError( cx, "closing printer" );
   jsval vPrinter ;
   if( JS_GetProperty( cx, glob, "printer", &vPrinter ) 
       &&
       JSVAL_IS_OBJECT( vPrinter ) )
   {
      JSObject *rhObj = JSVAL_TO_OBJECT( vPrinter );
      if( 0 <= printerFd_ )
      {
         close( printerFd_ );
         return true ;
      }
      else
         JS_ReportError( cx, "Invalid printer handle %d", printerFd_ );
   }
   else
      JS_ReportError( cx, "Error getting printer object" );
}

