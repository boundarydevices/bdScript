/*
 * Module jsPopen.cpp
 *
 * This module defines the JavaScript wrapper for 
 * the popen system call.
 *
 *
 * Change History : 
 *
 * $Log: jsPopen.cpp,v $
 * Revision 1.1  2002-12-10 04:48:30  ericn
 * -added module jsPopen
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsPopen.h"
#include "popen.h"

static JSBool
jsPopen( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 1 <= argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sCmdLine = JSVAL_TO_STRING( argv[0] );
      std::string cmdLine( JS_GetStringBytes( sCmdLine ), JS_GetStringLength( sCmdLine ) );
      popen_t proc( cmdLine.c_str() );
      if( proc.isOpen() )
      {
         JSObject *const returnArray = JS_NewArrayObject( cx, 0, NULL );
         *rval = OBJECT_TO_JSVAL( returnArray );
         unsigned long const timeout = ( ( 2 <= argc ) && JSVAL_IS_INT( argv[1] ) )
                                       ? JSVAL_TO_INT( argv[1] )
                                       : 0xFFFFFFFF ;
         std::string nextLine ;
         jsint numLines = 0 ;
         while( proc.getLine( nextLine, timeout ) )
         {
            JSString * const sLine = JS_NewStringCopyN( cx, nextLine.c_str(), nextLine.size() );
            jsval vLine = STRING_TO_JSVAL( sLine );
            JS_SetElement( cx, returnArray, numLines++, &vLine );
         }
      }
      else
      {
         *rval = STRING_TO_JSVAL( JS_NewStringCopyZ( cx, proc.getErrorMsg().c_str() ) );
      }
   }
   else
      JS_ReportError( cx, "Usage : popen( 'cmd' [,timeoutms=0xffffffff] );" );

   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"popen",         jsPopen,      0 },
    {0}
};

bool initJSPopen( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}

