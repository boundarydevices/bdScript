/*
 * Module jsHyperlink.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: jsHyperlink.cpp,v $
 * Revision 1.6  2003-07-06 01:27:10  ericn
 * -modified to wake code puller thread
 *
 * Revision 1.5  2003/02/27 03:51:02  ericn
 * -added exec routine
 *
 * Revision 1.4  2003/01/31 13:29:28  ericn
 * -added currentURL()
 *
 * Revision 1.3  2003/01/12 03:03:52  ericn
 * -added currentURL() method
 *
 * Revision 1.2  2002/12/01 02:40:38  ericn
 * -made gotoCalled_ volatile
 *
 * Revision 1.1  2002/10/27 17:42:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsHyperlink.h"
#include "relativeURL.h"
#include "codeQueue.h"

bool volatile gotoCalled_ = false ;
bool volatile execCalled_ = false ;
std::string gotoURL_( "" );
std::string execCmd_( "" );

static JSBool
jsGoto( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 <= argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      gotoCalled_ = true ;
      abortCodeQueue();
      absoluteURL( JS_GetStringBytes( JS_ValueToString( cx, argv[0] ) ), gotoURL_ );
   }

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}


static JSBool
jsExec( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 <= argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      execCalled_ = true ;
      abortCodeQueue();
      execCmd_ = JS_GetStringBytes( JS_ValueToString( cx, argv[0] ) );
   }

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec text_functions[] = {
    {"gotoURL",          jsGoto,       1 },
    {"exec",             jsExec,       1 },
    {0}
};


bool initJSHyperlink( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, text_functions);
}

