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
 * Revision 1.7  2002-11-05 05:43:05  ericn
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
#include "curlCache.h"
#include "childProcess.h"
#include <unistd.h>
#include <list>
#include "js/jscntxt.h"
#include "curlThread.h"
#include "mad.h"

typedef struct playListEntry_t {
   std::string url_ ;
   std::string onComplete_ ;
   std::string onCancel_ ;
   std::string onError_ ;
};

typedef std::list<playListEntry_t> playList_t ;

static playList_t playList_ ;

class mp3Process_t : public childProcess_t {
public:
   virtual void    died( void );
   playListEntry_t details_ ;
};

void mp3Process_t :: died( void )
{
//   printf( "mp3 process died\n" );
   pid_ = -1 ;
   printf( "finished playback of %s\n", details_.url_.c_str() );

   if( !playList_.empty() )
   {
      details_ = playList_.front();
      playList_.pop_front();
      std::string const url = details_.url_ ;
//      printf( "starting %s\n", url.c_str() );
      char *args[3] = {
         "./mp3Play",
         (char *)url.c_str(),
         0
      };

      run( args[0], args, __environ );
   }
}

static mp3Process_t    curProcess_ ;

static JSBool
jsMP3Play( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 <= argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      curlCache_t &cache = getCurlCache();
      char *cURL = JS_GetStringBytes( JS_ValueToString( cx, argv[0] ) );
      curlFile_t f( cache.get( cURL, true ) );
      if( f.isOpen() )
      {
         std::string onComplete ;
         std::string onCancel ;
         std::string onError ;

         if( ( 2 <= argc ) && JSVAL_IS_STRING( argv[1] ) )
         {
            onComplete = JS_GetStringBytes( JS_ValueToString( cx, argv[1] ) );
            if( ( 3 <= argc ) && JSVAL_IS_STRING( argv[2] ) )
            {
               onCancel = JS_GetStringBytes( JS_ValueToString( cx, argv[2] ) );
               if( ( 4 <= argc ) && JSVAL_IS_STRING( argv[3] ) )
               {
                  onError = JS_GetStringBytes( JS_ValueToString( cx, argv[3] ) );
               }
            }
         }
//         printf( "mime type %s\n", f.getMimeType() );
         childProcessLock_t lock ;

         std::string absolute ;
         absoluteURL( cURL, absolute );
         
         playListEntry_t next ;
         next.url_        = absolute ;
         next.onComplete_ = onComplete ;
         next.onCancel_   = onCancel ;
         next.onError_    = onError ;

         if( !curProcess_.isRunning() )
         {
            curProcess_.details_ = next ;

            char *args[3] = {
               "./mp3Play",
               (char *)absolute.c_str(),
               0
            };

            if( curProcess_.run( args[0], args, __environ ) )
            {
//               printf( "playing %s\n", cURL );
            }
            else
            {
               printf( "Error %m running ./mp3Play %s\n", cURL );
            }
         }
         else
         {
//            printf( "queueing %s\n", absolute.c_str() );
            playList_.push_back( next );
         }

         *rval = JSVAL_TRUE ;
      }
      else
      {
         fprintf( stderr, "Error retrieving %s\n", cURL );
         *rval = JSVAL_FALSE ;
      }
   }
   else
   {
      printf( "usage : mp3Play( url )\n" );
      *rval = JSVAL_FALSE ;
   }
   
   return JS_TRUE ;
}

static JSBool
jsMP3Wait( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   while( 1 )
   {
      bool somethingQueued_ ;
      {
         childProcessLock_t lock ;
         somethingQueued_ = ( curProcess_.isRunning() || !playList_.empty() );
      }
      if( somethingQueued_ )
         pause();
      else
         break;
   }
   
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSBool
jsMP3Skip( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   childProcessLock_t lock ;
   if( curProcess_.isRunning() )
      kill( curProcess_.pid_, 9 );
   
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSBool
jsMP3Count( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   int numQueued = 0 ;

   {
      childProcessLock_t lock ;
      if( curProcess_.isRunning() )
         numQueued = 1 ;
   
      numQueued += playList_.size();
   }

   *rval = INT_TO_JSVAL( numQueued );
   return JS_TRUE ;
}

static JSFunctionSpec mp3_functions[] = {
    {"mp3Play",           jsMP3Play,        1 },
    {"mp3Wait",           jsMP3Wait,        1 },
    {"mp3Skip",           jsMP3Skip,        1 },
    {"mp3Count",          jsMP3Count,       1 },
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
  {0,0,0}
};

static void mp3OnComplete( jsCurlRequest_t &req, curlFile_t const &f )
{
   //
   // MP3 data is 
   //
   jsCurlOnComplete( req, f );
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
         
         jsCurlRequest_t request ;
         request.onComplete = jsCurlOnComplete ;
         request.onError    = jsCurlOnError ;
         request.lhObj_     = thisObj ;
         request.rhObj_     = rhObj ;
         request.cx_        = cx ;

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
         JS_ReportError( cx, "Error allocating mp3File" );
   }
   else
      JS_ReportError( cx, "Usage: new mp3File( {url:\"something\"} );" );
      
   return JS_FALSE ;

}

bool initJSMP3( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, mp3_functions);
}

