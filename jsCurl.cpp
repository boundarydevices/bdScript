/*
 * Module jsCurl.cpp
 *
 * This module defines the curl classes and routines, and
 * the initialization routine as declared and described in
 * jsCurl.h
 *
 *
 * Change History : 
 *
 * $Log: jsCurl.cpp,v $
 * Revision 1.10  2002-11-05 15:15:39  ericn
 * -fixed to initialize members
 *
 * Revision 1.9  2002/11/03 17:55:51  ericn
 * -modified to support synchronous gets and posts
 *
 * Revision 1.8  2002/10/31 02:10:46  ericn
 * -modified curlFile() constructor to be multi-threaded, use rh object
 *
 * Revision 1.7  2002/10/25 04:49:05  ericn
 * -removed debug statements
 *
 * Revision 1.6  2002/10/15 05:00:39  ericn
 * -added error messages
 *
 * Revision 1.5  2002/10/13 15:52:28  ericn
 * -made return value an object
 *
 * Revision 1.4  2002/10/13 14:36:13  ericn
 * -removed curlPost(), fleshed out variable handling
 *
 * Revision 1.3  2002/10/13 13:50:57  ericn
 * -merged curlGet() and curlPost() with curlFile object
 *
 * Revision 1.2  2002/10/06 14:54:10  ericn
 * -added Finalize, removed debug statements
 *
 * Revision 1.1  2002/09/29 17:36:23  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsCurl.h"
#include "js/jsstddef.h"
#include "js/jscntxt.h"
#include "js/jsapi.h"
#include "js/jslock.h"
#include "urlFile.h"
#include <stdio.h>
#include "curlCache.h"
#include "relativeURL.h"
#include "curlThread.h"
#include "jsGlobals.h"

enum jsCurl_tinyid {
   CURLFILE_ISLOADED, 
   CURLFILE_WORKED, 
   CURLFILE_DATA, 
   CURLFILE_URL, 
   CURLFILE_HTTPCODE, 
   CURLFILE_FILETIME, 
   CURLFILE_MIMETYPE,
   CURLFILE_PARAMS
};

extern JSClass jsCurlClass_ ;

JSClass jsCurlClass_ = {
  "curlFile",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec curlFileProperties_[] = {
  {"isLoaded",       CURLFILE_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"data",           CURLFILE_DATA,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"fileTime",       CURLFILE_FILETIME,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"mimeType",       CURLFILE_MIMETYPE,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static void curlFileOnComplete( jsCurlRequest_t &req, curlFile_t const &f )
{
   JSString *sData = JS_NewStringCopyN( req.cx_, (char const *)f.getData(), f.getSize() );
   JS_DefineProperty( req.cx_, req.lhObj_, "data",
                      STRING_TO_JSVAL( sData ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   JS_DefineProperty( req.cx_, req.lhObj_, "fileTime",
                      INT_TO_JSVAL( f.getFileTime() ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   JSString *sMime = JS_NewStringCopyZ( req.cx_, f.getMimeType() );
   JS_DefineProperty( req.cx_, req.lhObj_, "mimeType",
                      STRING_TO_JSVAL( sMime ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
   jsCurlOnComplete( req, f );
}

static JSBool curlFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = js_NewObject( cx, &jsCurlClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         
         JS_DefineProperty( cx, thisObj, "isLoaded",
                            JSVAL_FALSE,
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JS_DefineProperty( cx, thisObj, "initializer",
                            argv[0],
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JSObject *const rhObj = JSVAL_TO_OBJECT( argv[0] );
         
         jsCurlRequest_t request ;
         request.onComplete = curlFileOnComplete ;
         request.onError    = jsCurlOnError ;
         request.lhObj_ = thisObj ;
         request.rhObj_ = rhObj ;
         request.cx_    = cx ;

         if( queueCurlRequest( request, ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) ) ) )
         {
            return JS_TRUE ;
         }
         else
         {
            JS_ReportError( cx, "Error queueing curlRequest" );
         }
      }
      else
         JS_ReportError( cx, "Error allocating curlFile" );
   }
   else
      JS_ReportError( cx, "Usage: new curlFile( {url:\"something\"} );" );
      
   return JS_FALSE ;

}

bool initJSCurl( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsCurlClass_,
                       /* native constructor function and min arg count */
                       curlFile, 1,
                       curlFileProperties_, 0,
                       0, 0 );
   if( rval )
   {
      return true ;
   }
   
   return false ;
}


