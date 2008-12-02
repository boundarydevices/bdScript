/*
 * Module jsHyperlink.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: jsHyperlink.cpp,v $
 * Revision 1.7  2008-12-02 00:22:47  ericn
 * use exec, not system to launch another program
 *
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
std::vector<std::string> execCmdArgs_ ;

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
       JSVAL_IS_STRING( argv[0] ) ){
      execCalled_ = true ;
      abortCodeQueue();
      execCmd_ = JS_GetStringBytes( JS_ValueToString( cx, argv[0] ) );
      execCmdArgs_.clear();
      execCmdArgs_.push_back(execCmd_);
   }
   else if( (1 == argc) && JSVAL_IS_OBJECT(argv[0]) ){
      JSObject  *obj ;
      JSIdArray *elements ;

      if( JS_ValueToObject(cx, argv[0], &obj)
          &&
          ( 0 != ( elements = JS_Enumerate(cx,obj ) ) ) ){
         for( int i = 0 ; i < elements->length ; i++ )
         {
            jsval rv ;
            JSString *s ;
            if( JS_LookupElement( cx, obj, i, &rv )    //look up each element
                &&
                (0 != (s=JS_ValueToString(cx, rv))) )
            {
               execCmdArgs_.push_back(std::string(JS_GetStringBytes(s)));
            }
         }
      }
      else
         JS_ReportError(cx, "Error parsing exec() array parameter\n" );
      
      if( 0 < execCmdArgs_.size() ){
         execCmd_ = execCmdArgs_[0];
         execCalled_ = true ;
      }
   }
   else
      JS_ReportError( cx, "Usage: exec( 'cmdline' ) || exec( ['/path/to/exe','param1' ...] )\n" );

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

