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
 * Revision 1.19  2003-02-01 18:14:30  ericn
 * -added wave file playback support
 *
 * Revision 1.18  2002/12/15 20:01:25  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.17  2002/12/15 00:07:58  ericn
 * -removed debug msgs
 *
 * Revision 1.16  2002/12/03 13:36:13  ericn
 * -collapsed code and objects for curl transfers
 *
 * Revision 1.15  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.14  2002/11/30 16:26:33  ericn
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
#include "macros.h"
#include "hexDump.h"

static JSBool
jsMP3Play( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
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

      jsval onComplete = JSVAL_VOID ;
      jsval onCancel = JSVAL_VOID ;
      
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
             JSVAL_IS_STRING( val ) )
         {
            onComplete = val ;
         } // have onComplete handler
         
         if( JS_GetProperty( cx, rhObj, "onCancel", &val ) 
             &&
             JSVAL_IS_STRING( val ) )
         {
            onCancel = val ;
         } // have onComplete handler
      } // have right-hand object

      if( getAudioQueue().queuePlayback( obj, data, length, onComplete, onCancel ) )
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
   MP3FILE_PARAMS,
   MP3FILE_DURATION,
   MP3FILE_SAMPLERATE,
   MP3FILE_NUMCHANNELS,
   MP3FILE_FRAMECOUNT
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
  {"isLoaded",       MP3FILE_ISLOADED,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"data",           MP3FILE_DATA,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"duration",       MP3FILE_DURATION,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"sampleRate",     MP3FILE_SAMPLERATE,  JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"numChannels",    MP3FILE_NUMCHANNELS, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"frameCount",     MP3FILE_FRAMECOUNT,  JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static void mp3OnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
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
      JSObject *thisObj = JS_NewObject( cx, &jsMp3Class_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         if( queueCurlRequest( thisObj, argv[0], cx, mp3OnComplete ) )
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

static JSBool
jsWavePlay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
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

      jsval onComplete = JSVAL_VOID ;
      jsval onCancel   = JSVAL_VOID ;
      
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
             JSVAL_IS_STRING( val ) )
         {
            onComplete = val ;
         } // have onComplete handler
         
         if( JS_GetProperty( cx, rhObj, "onCancel", &val ) 
             &&
             JSVAL_IS_STRING( val ) )
         {
            onCancel = val ;
         } // have onComplete handler
      } // have right-hand object

      audioQueue_t::waveHeader_t const &header = *(audioQueue_t::waveHeader_t const *)data ;
      if( getAudioQueue().queuePlayback( obj, header, onComplete, onCancel ) )
      {
//         JS_ReportError( cx, "queued Wave for playback" );
      }
      else
      {
         JS_ReportError( cx, "Error queueing Wave for playback" );
      }
   }
   else
      JS_ReportError( cx, "Invalid Wave data" );

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec waveFileMethods_[] = {
    {"play",         jsWavePlay,           0 },
    {0}
};

enum jsWaveFile_tinyid {
   WAVEFILE_ISLOADED, 
   WAVEFILE_WORKED, 
   WAVEFILE_DATA, 
   WAVEFILE_URL, 
   WAVEFILE_HTTPCODE, 
   WAVEFILE_FILETIME, 
   WAVEFILE_MIMETYPE,
   WAVEFILE_PARAMS,
   WAVEFILE_DURATION,
   WAVEFILE_SAMPLERATE,
   WAVEFILE_NUMCHANNELS,
};

extern JSClass jsWaveFileClass_ ;

JSClass jsWaveFileClass_ = {
  "waveFile",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec waveFileProperties_[] = {
  {"isLoaded",       WAVEFILE_ISLOADED,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"data",           WAVEFILE_DATA,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"duration",       WAVEFILE_DURATION,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"sampleRate",     WAVEFILE_SAMPLERATE,  JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"numChannels",    WAVEFILE_NUMCHANNELS, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static unsigned long const riffLong = *( unsigned long const * )"RIFF" ;
static unsigned long const waveLong = *( unsigned long const * )"WAVE" ;
static unsigned long const fmtLong  = *( unsigned long const * )"fmt " ;
static unsigned long const dataLong = *( unsigned long const * )"data" ;

#define WAVE_FORMAT_PCM 0x0001

static void waveFileOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   //
   // Wave data is loaded in data[], validate and parse headers
   // Note that data[] may not be properly aligned, so we copy the header
   // portion to a 
   //
   unsigned long longs[12];
   if( size > sizeof( longs ) )
   {
      memcpy( longs, data, sizeof( longs ) );
      if( ( riffLong == longs[0] ) 
          &&
          ( waveLong == longs[2] ) 
          &&
          ( fmtLong == longs[3] ) 
          &&
          ( dataLong == longs[9] ) )
      {
         unsigned long const sampleRate = longs[6];
         unsigned long const bytesPerSecond = longs[7];
   
         if( ( 0 < sampleRate ) && ( bytesPerSecond > sampleRate ) )
         {
            unsigned short const *const words = (unsigned short const *)longs ;
            unsigned short const numChannels = words[11];
            if( ( WAVE_FORMAT_PCM == words[10] ) 
                && 
                ( ( 1 == numChannels ) || ( 2 == numChannels ) ) )
            {
               unsigned const bytesPerSample = bytesPerSecond/(numChannels*sampleRate);
               unsigned long const dataBytes = longs[10];
               if( ( size == dataBytes + 0x2c ) 
                   &&
                   ( 2 == bytesPerSample ) )
               {
                  //
                  // place waveHeader_t on the front of this
                  //
                  unsigned const headerSize = fieldoffs(audioQueue_t::waveHeader_t,samples_[0]);
                  unsigned const totalSize = size + headerSize - 0x2c ;
                  JSString *sData = JS_NewStringCopyN( req.cx_, ((char const *)data)+0x2c-headerSize, totalSize );
                  if( sData )
                  {
                     char *waveData = JS_GetStringBytes( sData );
                     audioQueue_t :: waveHeader_t &header = *( audioQueue_t :: waveHeader_t *)waveData ;
                     header.numChannels_ = numChannels ;
                     header.numSamples_  = dataBytes/(bytesPerSample+numChannels);
                     header.sampleRate_  = sampleRate ;
   
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
                                        INT_TO_JSVAL( dataBytes/bytesPerSecond ),
                                        0, 0, 
                                        JSPROP_ENUMERATE
                                        |JSPROP_PERMANENT
                                        |JSPROP_READONLY );
                     JS_DefineProperty( req.cx_, 
                                        req.lhObj_, 
                                        "sampleRate",
                                        INT_TO_JSVAL( sampleRate ),
                                        0, 0, 
                                        JSPROP_ENUMERATE
                                        |JSPROP_PERMANENT
                                        |JSPROP_READONLY );
                     JS_DefineProperty( req.cx_, 
                                        req.lhObj_, 
                                        "numChannels",
                                        INT_TO_JSVAL( numChannels ),
                                        0, 0, 
                                        JSPROP_ENUMERATE
                                        |JSPROP_PERMANENT
                                        |JSPROP_READONLY );
                     return ;
                  }
                  else
                     JS_ReportError( req.cx_, "Error copying audio data" );
               } // check data chunk size
               else
                  JS_ReportError( req.cx_, "Invalid wave data size" );
            }
            else
               JS_ReportError( req.cx_, "Unknown channel count or format" );
         }
         else
            JS_ReportError( req.cx_, "8-bit samples not (yet) supported" );
      }
      else
      {
         JS_ReportError( req.cx_, "parsing Wave headers\n" );
         printf( "data ptr %p/%08lx\n", data, *( unsigned long *)data );
         printf( "long ptr %p/%08lx\n", longs, longs[0] );
         printf( "%08lx/%08lx\n"
                 "%08lx/%08lx\n"
                 "%08lx/%08lx\n"
                 "%08lx/%08lx\n",
                 riffLong, longs[0], 
                 waveLong, longs[2], 
                 fmtLong, longs[3], 
                 dataLong, longs[9] );
      }

      hexDumper_t dump( longs, sizeof( longs ) );
      while( dump.nextLine() )
         printf( "%s\n", dump.getLine() );
   }
   else
      JS_ReportError( req.cx_, "WAVE too short" );
}

static JSBool waveFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsWaveFileClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         if( queueCurlRequest( thisObj, argv[0], cx, waveFileOnComplete ) )
         {
            return JS_TRUE ;
         }
         else
         {
            JS_ReportError( cx, "Error queueing curlRequest" );
         }
      }
      else
         JS_ReportError( cx, "Error allocating waveFile" );
   }
   else
      JS_ReportError( cx, "Usage: new waveFile( {url:\"something\"} );" );
      
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
      rval = JS_InitClass( cx, glob, NULL, &jsWaveFileClass_,
                           waveFile, 1,
                           waveFileProperties_, 
                           waveFileMethods_,
                           0, 0 );
      if( rval )
      {
         return JS_DefineFunctions( cx, glob, _functions);
      }
   }
   return false ;
}

