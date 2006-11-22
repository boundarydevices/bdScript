/*
 * Module jsUsblp.cpp
 *
 * This module defines the initialization routines
 * for the usblp Javascript class as declared and
 * described in jsUsblp.h
 *
 *
 * Change History : 
 *
 * $Log: jsUsblp.cpp,v $
 * Revision 1.2  2006-11-22 17:19:02  ericn
 * -allow skip of output, retry USB writes
 *
 * Revision 1.1  2006/10/29 21:59:10  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "jsUsblp.h"
#include "js/jscntxt.h"
#include "usblpPoll.h"
#include "jsGlobals.h"
#include "codeQueue.h"
#include "imageInfo.h"
#include "imageToPS.h"
#include <errno.h>

class jsUsblpPoll_t : public usblpPoll_t {
public:
   jsUsblpPoll_t( JSObject *devObj );
   ~jsUsblpPoll_t( void );

   virtual void onDataIn( void );
   int write( void const *data, int length );

   JSObject *obj_ ;
   bool      skipOutput_ ;
   FILE     *fLog_ ;
};

jsUsblpPoll_t::jsUsblpPoll_t( JSObject *devObj )
   : usblpPoll_t( pollHandlers_,
                  DEFAULT_USBLB_DEV,
                  2<<20,      // 2 MB
                  4096 )
   , obj_( devObj )
   , skipOutput_( false )
   , fLog_( 0 )
{
   JS_AddRoot( execContext_, &obj_ );
}

jsUsblpPoll_t::~jsUsblpPoll_t( void )
{
   JS_RemoveRoot( execContext_, &obj_ );
   if( fLog_ )
      fclose( fLog_ );
}

void jsUsblpPoll_t::onDataIn( void )
{
   jsval handler ;
   if( JS_GetProperty( execContext_, obj_, "onDataIn", &handler ) 
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( execContext_, handler ) ) )
   {
      executeCode( obj_, handler, "onDataIn", 0, 0 );
   } // have handler
   else
   {
      printf( "no raw data handler\n" );
      char temp[256];
      unsigned numRead ;
      while( read( temp, sizeof(temp), numRead ) )
         ;
   } // no handler defined
}

int jsUsblpPoll_t::write( void const *data, int length )
{
   if( skipOutput_ )
      return length ;
   else
      return usblpPoll_t::write(data,length);
}

static JSObject *lpProto = NULL ;

static void jsUsblpFinalize(JSContext *cx, JSObject *obj);

JSClass jsUsblpClass_ = {
  "usblp",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsUsblpFinalize,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

//
// constructor for the scanner object
//
static JSBool jsUsblp( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) )
   {
      JSObject *devObj = JS_NewObject( cx, &jsUsblpClass_, lpProto, obj );
   
      if( devObj )
      {
         *rval = OBJECT_TO_JSVAL(devObj); // root

         JS_SetPrivate( cx, devObj, new jsUsblpPoll_t(devObj) );
      }
      else
      {
         JS_ReportError( cx, "allocating usblp\n" );
         *rval = JSVAL_FALSE ;
      }
   }
   else
      JS_ReportError( cx, "Usage : mylp = new usblp();" );

   return JS_TRUE;
}

static void jsUsblpFinalize(JSContext *cx, JSObject *obj)
{
   jsUsblpPoll_t *dev = (jsUsblpPoll_t *)JS_GetInstancePrivate( cx, obj, &jsUsblpClass_, NULL );
   if( dev )
   {
      JS_SetPrivate( cx, obj, 0 );
      delete dev ;
   } // have button data
}

static JSBool
jsWrite( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsUsblpPoll_t *dev = (jsUsblpPoll_t *)JS_GetInstancePrivate( cx, obj, &jsUsblpClass_, NULL );
   if( dev )
   {
      unsigned totalLength = 0 ;
      for( uintN arg = 0 ; arg < argc ; arg++ )
      {
         JSString *str = JS_ValueToString(cx, argv[arg]);
         if( str ){
            unsigned segLength = JS_GetStringLength( str );
            char const *outData = JS_GetStringBytes( str );
            while( 0 < segLength ){
               int numWritten = dev->write( outData, segLength );
               if( 0 < numWritten ){
                  totalLength += numWritten ;
                  if( dev->fLog_ ){
                     fwrite( outData, 1, numWritten, dev->fLog_ );
                  }
                  segLength -= numWritten ;
                  outData += numWritten ;
               }
               else {
                  JS_ReportError( cx, "usblp: short write %d of %u\n", numWritten, segLength );
                  break ;
               }
            }
         }
         else
            JS_ReportError( cx, "usblp.write() - error converting to string\n" );
      }
      *rval = INT_TO_JSVAL(totalLength);
   }
   else
      JS_ReportError( cx, "Invalid usblp object\n" );
   
   return JS_TRUE ;
}

static JSBool
jsRead( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsUsblpPoll_t *dev = (jsUsblpPoll_t *)JS_GetInstancePrivate( cx, obj, &jsUsblpClass_, NULL );
   if( dev )
   {
      unsigned numAvail = dev->bytesAvail();
      if( 0 < numAvail ){
         char *const inBuf = (char *)JS_malloc( cx, numAvail+1 );
         unsigned numRead ;
         if( dev->read( inBuf, numAvail, numRead ) ){
            JSString *sData = JS_NewString( cx, inBuf, numRead );
            *rval = STRING_TO_JSVAL( sData );
         }
         else {
            JS_ReportError( cx, "usblp read error\n" );
            JS_free(cx, inBuf);
         }
      }
   }
   else
      JS_ReportError( cx, "Invalid usblp object\n" );
   return JS_TRUE ;
}

static bool imagePsOutput( char const *outData,
                           unsigned    outLength,
                           void       *opaque )
{
   jsUsblpPoll_t *dev = (jsUsblpPoll_t *)opaque ;
   
   while( 0 < outLength ){
      int numWritten = dev->write( outData, outLength );
      if( 0 < numWritten ){
         if( dev->fLog_ ){
            fwrite( outData, 1, numWritten, dev->fLog_ );
         }
         outData += numWritten ;
         outLength -= numWritten ;
      }
      else {
         fprintf( stderr, "usblp write error %d: %m\n", numWritten );
         return false ;
      }
   }
   
   return true ;
}


static JSBool
jsImageToPS( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   static char const usage[] = {
      "Usage: lp.imageToPS( { image: data, x:0, y:0, w:10, h:10 } ) );\n" 
   };

   *rval = JSVAL_FALSE ;
   jsUsblpPoll_t *dev = (jsUsblpPoll_t *)JS_GetInstancePrivate( cx, obj, &jsUsblpClass_, NULL );
   if( dev )
   {
      JSObject *paramObj ;
      if( ( 1 == argc ) 
          && 
          JSVAL_IS_OBJECT(argv[0])
          &&
          ( 0 != ( paramObj = JSVAL_TO_OBJECT(argv[0]) ) ) )
      {
         JSString *sImageData ;
         unsigned x, y ;
         jsval val ;
         if( JS_GetProperty( cx, paramObj, "x", &val ) 
             && 
             ( x = JSVAL_TO_INT(val), JSVAL_IS_INT( val ) )
             &&
             JS_GetProperty( cx, paramObj, "y", &val ) 
             && 
             ( y = JSVAL_TO_INT(val), JSVAL_IS_INT( val ) )
             &&
             JS_GetProperty( cx, paramObj, "image", &val ) 
             && 
             JSVAL_IS_STRING( val )
             &&
             ( 0 != ( sImageData = JSVAL_TO_STRING( val ) ) ) )
         {
            char const *const cImage = JS_GetStringBytes( sImageData );
            unsigned const    imageLen = JS_GetStringLength( sImageData );
            imageInfo_t imInfo ;
            if( getImageInfo( cImage, imageLen, imInfo ) ){
               unsigned w = imInfo.width_ ;
               unsigned h = imInfo.height_ ;
               if( JS_GetProperty( cx, paramObj, "w", &val ) && JSVAL_IS_INT( val ) )
                  w = JSVAL_TO_INT( val );
               if( JS_GetProperty( cx, paramObj, "h", &val ) && JSVAL_IS_INT( val ) )
                  h = JSVAL_TO_INT( val );

               rectangle_t r ;
               r.xLeft_ = x ;
               r.yTop_ = y ;
               r.width_ = w ;
               r.height_ = h ;

               if( imageToPS( cImage, imageLen,
                              r, imagePsOutput, dev ) ){
                  *rval = JSVAL_TRUE ;
               }
               else
                  JS_ReportError( cx, "imageToPS: write cancelled\n" );
            }
            else
               JS_ReportError( cx, "imageToPS: Invalid or unsupported image\n" );
         }
         else
            JS_ReportError( cx, usage );
      }
      else
         JS_ReportError( cx, usage );
   }
   else
      JS_ReportError( cx, "Invalid usblp object\n" );
   return JS_TRUE ;
}


static JSBool
jsStartLog( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   jsUsblpPoll_t *dev = (jsUsblpPoll_t *)JS_GetInstancePrivate( cx, obj, &jsUsblpClass_, NULL );
   if( dev )
   {
      JSString *path ;
      if( ( 1 <= argc )
          &&
          JSVAL_IS_STRING(argv[0])
          &&
          ( 0 != ( path = JSVAL_TO_STRING(argv[0]) ) ) )
      {
         if( dev->fLog_ )
            fclose( dev->fLog_ );
         dev->fLog_ = fopen( JS_GetStringBytes(path), "wb" );
         if( dev->fLog_ ){
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "startLog(\"%s\"): %s\n", JS_GetStringBytes(path), strerror(errno));
         if( ( 1 < argc ) && JSVAL_IS_BOOLEAN(argv[1]) ){
            dev->skipOutput_ = ( 0 != JSVAL_TO_BOOLEAN( argv[1] ) );
            printf( dev->skipOutput_ ? "skipping USB output\n" : "performing USB output\n" );
         }
      }
      else
         JS_ReportError( cx, "Usage: lp.startLog( path, [skipOutput=false] )\n" );
   }
   else
      JS_ReportError( cx, "Invalid usblp object\n" );
   return JS_TRUE ;
}

static JSBool
jsStopLog( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   jsUsblpPoll_t *dev = (jsUsblpPoll_t *)JS_GetInstancePrivate( cx, obj, &jsUsblpClass_, NULL );
   if( dev )
   {
      if( 0 == argc )
      {
         if( dev->fLog_ ){
            fclose( dev->fLog_ );
            dev->fLog_ = 0 ;
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "usblp: log not open\n" );
      }
      else
         JS_ReportError( cx, "Usage: lp.stopLog()\n" );
   }
   else
      JS_ReportError( cx, "Invalid usblp object\n" );
   
   return JS_TRUE ;
}

static JSFunctionSpec usblp_methods[] = {
   { "write",           jsWrite,     0,0,0 },
   { "read",            jsRead,      0,0,0 },
   { "imageToPS",       jsImageToPS, 0,0,0 },
   { "startLog",        jsStartLog,  0,0,0 },
   { "stopLog",         jsStopLog,   0,0,0 },
   { 0 }
};


static JSPropertySpec lpProperties_[] = {
  {0,0,0}
};

bool initJSUsbLp( JSContext *cx, JSObject *glob )
{
   lpProto = JS_InitClass( cx, glob, NULL, &jsUsblpClass_,
                           jsUsblp, 1, lpProperties_, usblp_methods,
                           0, 0 );
   if( lpProto )
   {
      JS_AddRoot( cx, &lpProto );
      return true ;
   }
   else
      JS_ReportError( cx, "initializing scanner class" );

   return false ;
}

