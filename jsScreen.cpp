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
 * Revision 1.2  2002-10-20 16:30:49  ericn
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

static JSFunctionSpec image_functions[] = {
    {"clearScreen",      jsClearScreen, 1 },
    {0}
};


bool initJSScreen( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, image_functions);
}

