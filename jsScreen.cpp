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
 * Revision 1.3  2002-10-31 02:08:10  ericn
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

