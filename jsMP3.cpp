/*
 * Module jsMP3.cpp
 *
 * This module defines the initialization routine and 
 * javascript support routines for playing MP3 audio
 * as declared in jsMP3.h
 *
 *
 * Change History : 
 *
 * $Log: jsMP3.cpp,v $
 * Revision 1.14  2002-11-30 16:26:33  ericn
 * -better error checking, new curl interface
 *
 * Revision 1.13  2002/11/30 05:30:16  ericn
 * -modified to expect call from default curl hander to app-specific
 *
 * Revision 1.12  2002/11/30 00:31:37  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.11  2002/11/14 14:24:19  ericn
 * -added mp3Cancel() routine
 *
 * Revision 1.10  2002/11/07 14:51:07  ericn
 * -removed debug msg
 *
 * Revision 1.9  2002/11/07 02:14:34  ericn
 * -removed bare functions
 *
 * Revision 1.8  2002/11/05 15:16:07  ericn
 * -more implementation of mp3File class and function
 *
 * Revision 1.7  2002/11/05 05:43:05  ericn
 * -partial implementation of mp3File class
 *
 * Revision 1.6  2002/10/27 17:39:11  ericn
 * -preliminary event-handling code
 *
 * Revision 1.5  2002/10/25 14:19:11  ericn
 * -added mp3Skip() and mp3Count() routines
 *
 * Revision 1.4  2002/10/25 03:00:56  ericn
 * -fixed return value
 *
 * Revision 1.3  2002/10/25 02:57:04  ericn
 * -removed debug print statements
 *
 * Revision 1.2  2002/10/25 02:53:14  ericn
 * -moved MP3 decoding to sub-process
 *
 * Revision 1.1  2002/10/24 13:19:03  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsMP3.h"
#include "relativeURL.h"
#include <unistd.h>
#include <list>
#include "js/jscntxt.h"
#include "jsCurl.h"
#include "mad.h"
#include "madHeaders.h"
#include "audioQueue.h"

static JSBool
jsMP3Play( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
printf( "playing file\n" );
   jsval                dataVal ;
   JSString            *dataStr ;
   unsigned char const *data ;
   if( JS_GetProperty( cx, obj, "data", &dataVal ) 
       &&
       JSVAL_IS_STRING( dataVal ) 
       &&
       ( 0 != ( dataStr = JSVAL_TO_STRING( dataVal ) ) ) 
       &&
       ( 0 != ( data = (unsigned char *)JS_GetStringBytes( dataStr ) ) ) )
   {
      unsigned const length = JS_GetStringLength( dataStr );

      std::string onComplete ;
      std::string onCancel ;
      
      JSObject *rhObj ;
      if( ( 1 <= argc )
          &&
          JSVAL_IS_OBJECT( argv[0] ) 
          &&
          ( 0 != ( rhObj = JSVAL_TO_OBJECT( argv[0] ) ) ) )
      {
         jsval     val ;
         JSString *sHandler ;
         
         if( JS_GetProperty( cx, rhObj, "onComplete", &val ) 
             &&
             JSVAL_IS_STRING( val ) 
             &&
             ( 0 != ( sHandler = JSVAL_TO_STRING( val ) ) ) )
         {
            onComplete = JS_GetStringBytes( sHandler );
         } // have onComplete handler
         
         if( JS_GetProperty( cx, rhObj, "onCancel", &val ) 
             &&
             JSVAL_IS_STRING( val ) 
             &&
             ( 0 != ( sHandler = JSVAL_TO_STRING( val ) ) ) )
         {
            onCancel = JS_GetStringBytes( sHandler );
         } // have onComplete handler
      } // have right-hand object
      
      if( getAudioQueue().insert( obj, data, length, onComplete, onCancel ) )
      {
//         JS_ReportError( cx, "queued MP3 for playback" );
      }
      else
      {
         JS_ReportError( cx, "Error queueing MP3 for playback" );
      }
   }
   else
      JS_ReportError( cx, "Invalid MP3 data" );

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec mp3Methods_[] = {
    {"play",         jsMP3Play,           0 },
    {0}
};

enum jsMp3_tinyid {
   MP3FILE_ISLOADED, 
   MP3FILE_WORKED, 
   MP3FILE_DATA, 
   MP3FILE_URL, 
   MP3FILE_HTTPCODE, 
   MP3FILE_FILETIME, 
   MP3FILE_MIMETYPE,
   MP3FILE_PARAMS
};

extern JSClass jsMp3Class_ ;

JSClass jsMp3Class_ = {
  "mp3File",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec mp3FileProperties_[] = {
  {"isLoaded",       MP3FILE_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"data",           MP3FILE_DATA,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"duration",       MP3FILE_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"frameCount",     MP3FILE_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"sampleRate",     MP3FILE_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"numChannels",    MP3FILE_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static void mp3OnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   printf( "mp3OnComplete\n" );
   //
   // MP3 data is loaded in data[], validate and parse headers
   //
   madHeaders_t headers( data, size );
   if( headers.worked() )
   {
      JSString *sData = JS_NewStringCopyN( req.cx_, (char const *)data, size );
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "data",
                         STRING_TO_JSVAL( sData ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "duration",
                         INT_TO_JSVAL( headers.lengthSeconds() ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "frameCount",
                         INT_TO_JSVAL( headers.frames().size() ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "sampleRate",
                         INT_TO_JSVAL( headers.playbackRate() ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "numChannels",
                         INT_TO_JSVAL( headers.numChannels() ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
   }
   else
   {
      JS_ReportError( req.cx_, "parsing MP3 headers\n" );
   }
}

static JSBool mp3File( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = js_NewObject( cx, &jsMp3Class_, NULL, NULL );

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
         
printf( "retrieving MP3\n" );
         if( queueCurlRequest( thisObj, rhObj, cx, ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) ), mp3OnComplete ) )
         {
            return JS_TRUE ;
         }
         else
         {
            JS_ReportError( cx, "Error queueing curlRequest" );
         }
printf( "done\n" );
      }
      else
         JS_ReportError( cx, "Error allocating mp3File" );
   }
   else
      JS_ReportError( cx, "Usage: new mp3File( {url:\"something\"} );" );
      
   return JS_FALSE ;

}

static JSBool
jsMP3Cancel( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   unsigned numCancelled ;
   if( getAudioQueue().clear( numCancelled ) )
   {
      *rval = INT_TO_JSVAL( numCancelled );
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"mp3Cancel",           jsMP3Cancel,          0 },
    {0}
};


bool initJSMP3( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsMp3Class_,
                                  mp3File, 1,
                                  mp3FileProperties_, 
                                  mp3Methods_,
                                  0, 0 );
   if( rval )
   {
      return JS_DefineFunctions( cx, glob, _functions);
   }
   else
      return false ;
}

