/*
 * Module jsMPEG.cpp
 *
 * This module defines the initialization routine and
 * internal calls for the mpegFile() class as declared
 * and described in jsMPEG.h
 *
 *
 * Change History : 
 *
 * $Log: jsMPEG.cpp,v $
 * Revision 1.2  2003-07-30 20:28:10  ericn
 * -removed decoder debug parameter
 *
 * Revision 1.1  2003/07/30 20:26:36  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsMPEG.h"
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
#include "mpDemux.h"
#include "mpegDecode.h"
#include <zlib.h>

static JSBool
jsMPEGPlay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
printf( "Playing video\n" ); fflush( stdout ); sleep( 2 );

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
      
      unsigned xPos = 0 ;
      unsigned yPos = 0 ;

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
         
         if( JS_GetProperty( cx, rhObj, "x", &val ) 
             &&
             JSVAL_IS_INT( val ) )
         {
            xPos = JSVAL_TO_INT( val );
         } // have x position
         
         if( JS_GetProperty( cx, rhObj, "y", &val ) 
             &&
             JSVAL_IS_INT( val ) )
         {
            yPos = JSVAL_TO_INT( val );
         } // have y position
      } // have right-hand object

      if( getAudioQueue().queueMPEG( obj, data, length, onComplete, onCancel, xPos, yPos ) )
      {
//         JS_ReportError( cx, "queued MPEG for playback" );
      }
      else
      {
         JS_ReportError( cx, "Error queueing MPEG for playback" );
      }
   }
   else
      JS_ReportError( cx, "Invalid MPEG data" );

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec mpegMethods_[] = {
    {"play",         jsMPEGPlay,           0 },
    {0}
};

enum jsMpeg_tinyid {
   MPEGFILE_ISLOADED, 
   MPEGFILE_WORKED, 
   MPEGFILE_DATA, 
   MPEGFILE_URL, 
   MPEGFILE_HTTPCODE, 
   MPEGFILE_FILETIME, 
   MPEGFILE_MIMETYPE,
   MPEGFILE_PARAMS,
   MPEGFILE_DURATION,
   MPEGFILE_STREAMCOUNT,
   MPEGFILE_NUMFRAMES,
   MPEGFILE_WIDTH,
   MPEGFILE_HEIGHT
};

extern JSClass jsMpegClass_ ;

JSClass jsMpegClass_ = {
  "mpegFile",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec mpegFileProperties_[] = {
  {"isLoaded",       MPEGFILE_ISLOADED,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"data",           MPEGFILE_DATA,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"duration",       MPEGFILE_DURATION,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"streamCount",    MPEGFILE_STREAMCOUNT, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"numFrames",      MPEGFILE_NUMFRAMES,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"width",          MPEGFILE_WIDTH,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"height",         MPEGFILE_HEIGHT,      JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static char const * const numFrameIndeces[] = {
   "video",
   "audio",
};

static void mpegOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   JSString *sData = JS_NewStringCopyN( req.cx_, (char const *)data, size );
   if( sData )
   {
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "data",
                         STRING_TO_JSVAL( sData ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      data = JS_GetStringBytes( sData );
      
      mpegDemux_t demux( data, size );
      mpegDemux_t::bulkInfo_t const *bi = demux.getFrames();
   
      if( 0 != bi )
      {
         JS_DefineProperty( req.cx_, 
                            req.lhObj_, 
                            "duration",
                            INT_TO_JSVAL( bi->msTotal_ ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JS_DefineProperty( req.cx_, 
                            req.lhObj_, 
                            "streamCount",
                            INT_TO_JSVAL( bi->count_ ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         
         unsigned width = 0, height = 0 ;
         
         JSObject *const numFramesObj = JS_NewObject( req.cx_, 0, 0, 0 );
         if( numFramesObj )
         {
            JS_DefineProperty( req.cx_, 
                               req.lhObj_, 
                               "numFrames",
                               OBJECT_TO_JSVAL( numFramesObj ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            unsigned totalFrames = 0 ;
            for( unsigned char sIdx = 0 ; sIdx < bi->count_ ; sIdx++ )
            {
               mpegDemux_t::streamAndFrames_t const &sAndF = *bi->streams_[sIdx];
   
               if( mpegDemux_t::videoFrame_e == sAndF.sInfo_.type )
               {
printf( "decoding video to find dimensions\n" ); fflush( stdout ); sleep( 1 );
                  mpegDecoder_t decoder ;
                  for( unsigned i = 0 ; ( 0 == width ) && ( i < sAndF.numFrames_ ); i++ )
                  {
                     mpegDemux_t::frame_t const &frame = sAndF.frames_[i];
                     decoder.feed( frame.data_, frame.length_ );
                     void const *picture ;
                     mpegDecoder_t::picType_e type ;
// if( 10 < i ){ printf( "getPicture %u\n", i ); fflush( stdout ); sleep( 1 ); }
                     while( decoder.getPicture( picture, type, 0 ) )
                     {
                        if( decoder.haveHeader() )
                        {
                           width = decoder.width();
                           height = decoder.height();
printf( "have size %u x %u\n", width, height ); fflush( stdout ); sleep( 1 );
                           break;
                        }
                     }
                  } // keep passin frames til we have video size               
                  printf( "width %u, height %u\n", width, height );
               } // video stream
   
               
               JS_DefineProperty( req.cx_, numFramesObj, numFrameIndeces[sAndF.sInfo_.type], 
                                  INT_TO_JSVAL( sAndF.numFrames_ ),
                                  0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               totalFrames += sAndF.numFrames_ ;
            }
            
            JS_DefineProperty( req.cx_, numFramesObj, "total", 
                               INT_TO_JSVAL( totalFrames ),
                               0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
         }
   
         JS_DefineProperty( req.cx_, 
                            req.lhObj_, 
                            "width",
                            INT_TO_JSVAL( width ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
   
         JS_DefineProperty( req.cx_, 
                            req.lhObj_, 
                            "height",
                            INT_TO_JSVAL( height ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
      }
      else
      {
         JS_ReportError( req.cx_, "parsing MPEG headers\n" );
      }
   }
   else
      JS_ReportError( req.cx_, "Error copying MPEG data\n" );
   
}

static JSBool mpegFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsMpegClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         if( queueCurlRequest( thisObj, argv[0], cx, mpegOnComplete ) )
         {
            return JS_TRUE ;
         }
         else
         {
            JS_ReportError( cx, "Error queueing curlRequest" );
         }
      }
      else
         JS_ReportError( cx, "Error allocating mpegFile" );
   }
   else
      JS_ReportError( cx, "Usage: new mpegFile( {url:\"something\"} );" );
      
   return JS_FALSE ;

}

bool initJSMPEG( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsMpegClass_,
                                  mpegFile, 1,
                                  mpegFileProperties_, 
                                  mpegMethods_,
                                  0, 0 );
   return ( 0 != rval );
}


