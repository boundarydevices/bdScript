/*
 * Module jsTTY.cpp
 *
 * This module defines the Javascript interface
 * routines for the TTY class and tty global as
 * declared and described in jsTTY.h
 *
 *
 * Change History : 
 *
 * $Log: jsTTY.cpp,v $
 * Revision 1.4  2003-12-28 16:00:04  ericn
 * -got rid of secondary thread
 *
 * Revision 1.3  2003/01/05 01:58:15  ericn
 * -added identification of threads
 *
 * Revision 1.2  2003/01/03 16:54:30  ericn
 * -fixed scope
 *
 * Revision 1.1  2002/12/27 23:31:14  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsTTY.h"
#include <string>
#include "js/jsstddef.h"
#include "js/jscntxt.h"
#include "js/jsapi.h"
#include "js/jslock.h"
#include "jsImage.h"
#include "mtQueue.h"
#include <stdio.h>
#include <ctype.h>
#include "codeQueue.h"
#include "ttyPoll.h"
#include "jsGlobals.h"
#include "jsExit.h"

static JSObject *ttyProto = NULL ;
static std::string prompt( "js:" );
static jsval onLineInCode_ = JSVAL_VOID ;
static jsval onLineInScope_ = JSVAL_VOID ;
static mtQueue_t<std::string> lineQueue_ ;

class jsTTY_t : public ttyPollHandler_t {
public:
   jsTTY_t( void )
      : ttyPollHandler_t( pollHandlers_, 0 ){}

   virtual void onLineIn( void );
   virtual void onCtrlC( void );
};

void jsTTY_t :: onLineIn( void )
{
   if( ( JSVAL_VOID != onLineInCode_ ) && ( JSVAL_VOID != onLineInScope_ ) )
   {
      lineQueue_.push( getLine() );
      executeCode( JSVAL_TO_OBJECT( onLineInScope_ ), onLineInCode_, "tty.onLineIn" );
   }
}

void jsTTY_t :: onCtrlC( void )
{
   exitStatus_ = 3 ;
   exitRequested_ = true ;
}

static jsTTY_t *tty_ = 0 ;


static JSBool
jsPrompt( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *str = JSVAL_TO_STRING( argv[0] );
      prompt.assign( JS_GetStringBytes( str ), JS_GetStringLength( str ) );
   }
   else
      JS_ReportError( cx, "usage: tty.prompt( string )" );

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSBool
jsReadln( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( 0 == argc )
   {
      std::string line ;
      if( lineQueue_.pull( line, 0 ) )
      {
         JSString *str = JS_NewStringCopyN( cx, line.c_str(), line.size() );
         *rval = STRING_TO_JSVAL( str );
      }
   }
   else
      JS_ReportError( cx, "usage: tty.readln()" );

   return JS_TRUE ;
}

static JSBool
jsOnLineIn( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      onLineInScope_ = OBJECT_TO_JSVAL( obj );
      onLineInCode_ = argv[0];
   }
   else
      JS_ReportError( cx, "usage: tty.onLineIn( code )" );

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSBool
jsWrite( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_VOID ;
   for( uintN arg = 0 ; arg < argc ; arg++ )
   {
      JSString *str = JS_ValueToString(cx, argv[arg]);
      if( str )
         fwrite( JS_GetStringBytes( str ), JS_GetStringLength( str ), 1, stdout );
   }
   
   fflush( stdout );

   return JS_TRUE ;
}

static JSBool
jsWriteln( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_VOID ;
   for( uintN arg = 0 ; arg < argc ; arg++ )
   {
      JSString *str = JS_ValueToString(cx, argv[arg]);
      if( str )
         fwrite( JS_GetStringBytes( str ), JS_GetStringLength( str ), 1, stdout );
   }
   fwrite( "\n", 1, 1, stdout );   
   fflush( stdout );

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec methods_[] = {
   { "prompt",       jsPrompt,           0,0,0 },
   { "readln",       jsReadln,           0,0,0 },
   { "onLineIn",     jsOnLineIn,         0,0,0 },
   { "write",        jsWrite,            0,0,0 },
   { "writeln",      jsWriteln,          0,0,0 },
   { 0 }
};

JSClass jsTTYClass_ = {
  "TTY",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec ttyProperties_[] = {
  {0,0,0}
};

static JSFunctionSpec functions_[] = {
    {"print",           jsWrite,        0},
    {0}
};


//
// constructor for the screen object
//
static JSBool jsTTY( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   obj = JS_NewObject( cx, &jsTTYClass_, NULL, NULL );

   if( obj )
   {
      *rval = OBJECT_TO_JSVAL(obj);
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

bool initJSTTY( JSContext *cx, JSObject *glob )
{
   ttyProto = JS_InitClass( cx, glob, NULL, &jsTTYClass_,
                            jsTTY, 1,
                            ttyProperties_, 
                            methods_,
                            0, 0 );
   if( ttyProto )
   {
      JS_AddRoot( cx, &ttyProto );

      if( JS_DefineFunctions( cx, glob, functions_ ) )
      {
         JSObject *console = JS_NewObject( cx, &jsTTYClass_, ttyProto, glob );
         if( console )
         {
            JS_DefineProperty( cx, glob, "tty", 
                               OBJECT_TO_JSVAL( console ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_AddRoot( cx, &onLineInCode_ );
            JS_AddRoot( cx, &onLineInScope_ );
            tty_ = new jsTTY_t ;
         }
      }
   }
      
   return false ;
}

void shutdownTTY( void )
{
   delete tty_ ;
}

