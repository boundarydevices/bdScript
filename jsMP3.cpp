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
 * Revision 1.4  2002-10-25 03:00:56  ericn
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

typedef std::list<std::string> playList_t ;

static playList_t playList_ ;

class mp3Process_t : public childProcess_t {
public:
   virtual void died( void );
};

void mp3Process_t :: died( void )
{
//   printf( "mp3 process died\n" );
   pid_ = -1 ;
   if( !playList_.empty() )
   {
      std::string const url = playList_.front();
      playList_.pop_front();
//      printf( "starting %s\n", url.c_str() );
      char *args[3] = {
         "./mp3Play",
         (char *)url.c_str(),
         0
      };

      run( args[0], args, __environ );
   }
}

static mp3Process_t curProcess_ ;

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
//         printf( "mime type %s\n", f.getMimeType() );
         childProcessLock_t lock ;

         std::string absolute ;
         absoluteURL( cURL, absolute );

         if( !curProcess_.isRunning() )
         {
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
            playList_.push_back( absolute );
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

static JSFunctionSpec text_functions[] = {
    {"mp3Play",           jsMP3Play,        1 },
    {"mp3Wait",           jsMP3Wait,        1 },
    {0}
};


bool initJSMP3( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, text_functions);
}

