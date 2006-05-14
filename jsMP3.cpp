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
 * Revision 1.25  2006-05-14 14:45:13  ericn
 * -allow predecoded MP3, add timing routines
 *
 * Revision 1.24  2005/11/06 00:49:34  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.23  2003/09/22 02:01:17  ericn
 * -separated boost and changed record level params
 *
 * Revision 1.22  2003/09/15 02:22:25  ericn
 * -updated to allow setting of recordLevel and record amplification
 *
 * Revision 1.21  2003/02/02 19:38:26  ericn
 * -removed debug msg
 *
 * Revision 1.20  2003/02/02 13:46:17  ericn
 * -added recordBuffer support
 *
 * Revision 1.19  2003/02/01 18:14:30  ericn
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
#include "madDecode.h"

// #define DEBUGPRINT
#include "debugPrint.h"

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
   jsval     val ;
   JSObject *initObj ;
   
   if( JS_GetProperty( req.cx_, req.lhObj_, initializerProp_, &val ) 
       &&
       JSVAL_IS_OBJECT( val ) 
       &&
       ( 0 != ( initObj = JSVAL_TO_OBJECT(val) ) ) )
   {
      if( JS_GetProperty( req.cx_, initObj, "predecode", &val ) 
          &&
          JSVAL_IS_INT( val ) 
          &&
          ( 0 != JSVAL_TO_INT(val) ) )
      {
         unsigned short *decoded ;
         unsigned        decodedBytes ;
         unsigned        sampleRate ;
         unsigned        numChannels ;
         if( madDecodeAll( data, size,
                           decoded, decodedBytes,
                           sampleRate, numChannels ) )
         {
            audioQueue_t::waveHeader_t *waveHdr ;
            
            unsigned waveSize = decodedBytes+sizeof(audioQueue_t::waveHeader_t)-sizeof(waveHdr->samples_);
            
            void *waveData = JS_malloc( req.cx_, waveSize );
            waveHdr = (audioQueue_t::waveHeader_t *)waveData ;
            waveHdr->numChannels_ = numChannels ;
            waveHdr->sampleRate_  = sampleRate ;
            waveHdr->numSamples_  = decodedBytes/sizeof(waveHdr->samples_[0]);

            memcpy( waveHdr->samples_, decoded, decodedBytes );

            JSString *sData = JS_NewString( req.cx_, (char *)waveData, waveSize );
            delete [] decoded ;

            JS_DefineProperty( req.cx_, 
                               req.lhObj_, 
                               "decoded",
                               STRING_TO_JSVAL( sData ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( req.cx_, 
                               req.lhObj_, 
                               "duration",
                               INT_TO_JSVAL( ( (decodedBytes*sizeof(unsigned short))+sampleRate-1)/sampleRate ),
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
         }
         else
            JS_ReportError( req.cx_, "decoding MP3 data\n" );
      }
      else
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
            JS_ReportError( req.cx_, "parsing MP3 headers\n" );
      }
   }
   else
      JS_ReportError( req.cx_, "Missing %s", initializerProp_ );
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

         hexDumper_t dump( longs, sizeof( longs ) );
         while( dump.nextLine() )
            printf( "%s\n", dump.getLine() );
      }
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
jsRecordPlay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
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
      jsval onComplete = JSVAL_VOID ;
      jsval onCancel   = JSVAL_VOID ;
      
      JSObject *rhObj ;
      if( ( 1 <= argc )
          &&
          JSVAL_IS_OBJECT( argv[0] ) 
          &&
          ( 0 != ( rhObj = JSVAL_TO_OBJECT( argv[0] ) ) ) )
      {
         jsval val ;
         
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
//         JS_ReportError( cx, "queued record buffer for playback" );
      }
      else
      {
         JS_ReportError( cx, "Error queueing record buffer for playback" );
      }
   }
   else
      JS_ReportError( cx, "Invalid record data" );

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSBool
jsRecord( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      jsval val ;
      if( !JS_GetProperty( cx, obj, "isRecording", &val ) 
          ||
          !JSVAL_IS_BOOLEAN( val ) 
          ||
          !JSVAL_TO_BOOLEAN( val ) )
      {
         if( JS_GetProperty( cx, obj, "data", &val ) )
         {
            int      sampleRate ;
            unsigned maxSamples ;
            JSString *dataStr = 0 ;
            if( !JSVAL_IS_STRING( val ) )
            {
               jsdouble maxDuration ;
               if( !JS_GetProperty( cx, obj, "maxSeconds", &val ) 
                   ||
                   !JSVAL_IS_NUMBER( val ) 
                   ||
                   !JS_ValueToNumber( cx, val, &maxDuration ) ) 
               {
                  maxDuration = 5.0 ;
               }
               
               if( !JS_GetProperty( cx, obj, "sampleRate", &val ) 
                   ||
                   !JSVAL_IS_INT( val ) 
                   ||
                   ( 0 == ( sampleRate = JSVAL_TO_INT( val ) ) ) ) 
               {
                  sampleRate = 11025 ;
               }
         
               maxSamples = (unsigned)( maxDuration*sampleRate );
               unsigned const bufSize = sizeof( audioQueue_t::waveHeader_t ) + maxSamples*sizeof(short);
               void *buf = JS_malloc( cx, bufSize );
               if( buf )
               {
                  dataStr = JS_NewString( cx, (char *)buf, bufSize );
                  if( dataStr )
                  {
                     JS_DefineProperty( cx, 
                                        obj, 
                                        "data",
                                        STRING_TO_JSVAL( dataStr ),
                                        0, 0, 
                                        JSPROP_ENUMERATE
                                        |JSPROP_PERMANENT
                                        |JSPROP_READONLY );
                     JS_DefineProperty( cx, 
                                        obj, 
                                        "maxSamples",
                                        INT_TO_JSVAL( maxSamples ),
                                        0, 0, 
                                        JSPROP_ENUMERATE
                                        |JSPROP_PERMANENT
                                        |JSPROP_READONLY );
                  }
                  else
                     JS_ReportError( cx, "rooting dataStr" );
               }
            } // need to allocate data buffer
            else
            {
               dataStr = JSVAL_TO_STRING( val );
               JS_GetProperty( cx, obj, "sampleRate", &val );
               sampleRate = JSVAL_TO_INT( val );
               JS_GetProperty( cx, obj, "maxSamples", &val );
               maxSamples = JSVAL_TO_INT( val );
            }
   
            if( dataStr )
            {
               JSObject *rhObj = JSVAL_TO_OBJECT( argv[0] );
               jsval onComplete = JSVAL_VOID ;
               jsval onCancel   = JSVAL_VOID ;
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
   
               char *buf = JS_GetStringBytes( dataStr );
               audioQueue_t::waveHeader_t &header = *( audioQueue_t::waveHeader_t * )buf ;
               header.numChannels_ = 1 ;
               header.sampleRate_  = sampleRate ;
               header.numSamples_  = maxSamples ;
               if( getAudioQueue().queueRecord( obj, header, onComplete, onCancel ) )
               {
                  JS_DefineProperty( cx, 
                                     obj, 
                                     "isRecording",
                                     JSVAL_TRUE,
                                     0, 0, 
                                     JSPROP_ENUMERATE
                                     |JSPROP_PERMANENT
                                     |JSPROP_READONLY );
                  *rval = JSVAL_TRUE ;
               }
               else
                  JS_ReportError( cx, "queueing record buffer" );
            }
         }
         else
            JS_ReportError( cx, "no record data" );
      }
      else
         JS_ReportError( cx, "already recording" );
   }
   else
      JS_ReportError( cx, "Usage: recordBuffer.record( { onComplete:\"code\", onCancel:\"code\" } );" );

   return JS_TRUE ;
}

static JSBool
jsRecordStop( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( getAudioQueue().stopRecording() )
      *rval = JSVAL_TRUE ;
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static JSBool
jsSetRecordLevel( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 == argc )
       &&
       ( JSVAL_IS_BOOLEAN( argv[0] ) )
       &&
       ( JSVAL_IS_INT( argv[1] ) ) )
   {
      bool const boost = JSVAL_TO_BOOLEAN( argv[0] );
      int const newLevel = JSVAL_TO_INT( argv[1] );
      getAudioQueue().setRecordLevel( boost, newLevel );
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Usage: recordBuffer.setRecordLevel( boost(bool), 0-31 );\n" );

   return JS_TRUE ;
}

static JSFunctionSpec recordBufferMethods_[] = {
    {"record",          jsRecord,               0 },
    {"playback",        jsRecordPlay,           0 },
    {"setRecordLevel",  jsSetRecordLevel,       0 },
    {"stop",            jsRecordStop,           0 },
    {0}
};

enum jsRecordFile_tinyid {
   RECORDBUFFER_DATA, 
   RECORDBUFFER_SAMPLERATE,
   RECORDBUFFER_MAXDURATION,
   RECORDBUFFER_MAXSAMPLES,
   RECORDBUFFER_SECSRECORDED,
   RECORDBUFFER_ISRECORDING,
};

extern JSClass jsRecordBufferClass_ ;

JSClass jsRecordBufferClass_ = {
  "recordBuffer",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec recordBufferProperties_[] = {
   {"data",           RECORDBUFFER_DATA,         JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
   {"sampleRate",     RECORDBUFFER_SAMPLERATE,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
   {"maxSeconds",     RECORDBUFFER_MAXDURATION,  JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
   {"maxSamples",     RECORDBUFFER_SAMPLERATE,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
   {"secsRecorded",   RECORDBUFFER_SECSRECORDED, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
   {"isRecording",    RECORDBUFFER_ISRECORDING,  JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
   {0,0,0}
};

static JSBool recordBuffer( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsRecordBufferClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root

         JSObject *rhObj ;
         if( 0 != ( rhObj = JSVAL_TO_OBJECT( argv[0] ) ) )
         {
            jsval  val ;
            double maxSeconds ;
            
            if( JS_GetProperty( cx, rhObj, "maxSeconds", &val ) 
                &&
                JSVAL_IS_NUMBER( val ) )
            {
               JS_ValueToNumber( cx, val, &maxSeconds );
            }
            else
               maxSeconds = 5.0 ;

            JS_DefineProperty( cx, 
                               thisObj, 
                               "maxSeconds",
                               DOUBLE_TO_JSVAL( JS_NewDouble( cx, maxSeconds ) ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            int sampleRate ;
            if( JS_GetProperty( cx, rhObj, "sampleRate", &val ) 
                &&
                JSVAL_IS_INT( val ) )
            {
               sampleRate = JSVAL_TO_INT( val );
            }
            else
               sampleRate = 11025 ;

            JS_DefineProperty( cx, 
                               thisObj, 
                               "sampleRate",
                               INT_TO_JSVAL( sampleRate ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );

            JS_DefineProperty( cx, 
                               thisObj, 
                               "secsRecorded",
                               DOUBLE_TO_JSVAL( JS_NewDouble( cx, 0.0 ) ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );

            JS_DefineProperty( cx, 
                               thisObj, 
                               "data",
                               JSVAL_NULL,
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            return JS_TRUE ;
         }
         else
            JS_ReportError( cx, "Error reading right-hand object" );
      }
      else
         JS_ReportError( cx, "Error allocating recordBuffer" );
   }
   else
      JS_ReportError( cx, "Usage: new recordBuffer( {sampleRate:x, maxSeconds:seconds} );" );

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

static JSBool
jsLastPlayStart( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = INT_TO_JSVAL( (lastPlayStart_.tv_sec*1000)+(lastPlayStart_.tv_usec / 1000) );

   return JS_TRUE ;
}

static JSBool
jsLastPlayEnd( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = INT_TO_JSVAL( (lastPlayEnd_.tv_sec*1000)+(lastPlayEnd_.tv_usec / 1000) );

   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"mp3Cancel",           jsMP3Cancel,     0 },
    {"lastPlayStart",       jsLastPlayStart, 0 },
    {"lastPlayEnd",         jsLastPlayEnd,   0 },
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
         rval = JS_InitClass( cx, glob, NULL, &jsRecordBufferClass_,
                              recordBuffer, 1,
                              recordBufferProperties_, 
                              recordBufferMethods_,
                              0, 0 );
         if( rval )
         {
            return JS_DefineFunctions( cx, glob, _functions);
         }
      }
   }
   return false ;
}

