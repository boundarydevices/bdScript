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
 * Revision 1.1  2002-10-18 01:18:25  ericn
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
   memset( fb.getMem(), 0, fb.getMemSize() );
         
   *rval = JSVAL_FALSE ;
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

