/*
 * Module jsProc.cpp
 *
 * This module defines the Javascript 
 *
 *
 * Change History : 
 *
 * $Log: jsProc.cpp,v $
 * Revision 1.1  2002-10-27 17:42:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsProc.h"

static JSBool
jsProc( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec text_functions[] = {
    {"jsProc",          jsProc,       1 },
    {0}
};


bool initJSProc( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, text_functions);
}

