/*
 * Module jsFlash.cpp
 *
 * This module defines the initialization routine for the
 * flashMovie class as declared and described in jsFlash.h
 *
 *
 * Change History : 
 *
 * $Log: jsFlash.cpp,v $
 * Revision 1.6  2003-11-28 14:57:41  ericn
 * -fixed shutdown dependencies by controlling parsedFlash_t lifetime, use prototype for construction
 *
 * Revision 1.5  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.4  2003/11/15 17:45:03  ericn
 * -modified to allow re-use
 *
 * Revision 1.3  2003/08/06 13:23:56  ericn
 * -fixed sound cancellation
 *
 * Revision 1.2  2003/08/04 12:39:01  ericn
 * -added sound support
 *
 * Revision 1.1  2003/08/03 19:27:56  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsFlash.h"
#include "relativeURL.h"
#include <unistd.h>
#include <list>
#include "js/jscntxt.h"
#include "jsCurl.h"
#include "macros.h"
#include "ccActiveURL.h"
#include "fbDev.h"
#include <pthread.h>
#include <linux/timer.h>
#include "semClasses.h"
#include "codeQueue.h"
#include "flashThread.h"
#include "parsedFlash.h"
#include "audioQueue.h"
#include "jsGlobals.h"
#include "debugPrint.h"

extern JSClass jsFlashClass_ ;
static JSObject *glob_ = 0 ;
static jsval     flashProto_ = JSVAL_VOID ;

class flashPollHandler_t : public pollHandler_t {
public:
   flashPollHandler_t( JSContext        *cx,
                       JSObject         *scope,
                       int               fd, 
                       pollHandlerSet_t &set )
      : pollHandler_t( fd, set )
      , scope_( scope )
      , scopeVal_( OBJECT_TO_JSVAL( scope ) )
      , onComplete_( JSVAL_VOID )
      , onCancel_( JSVAL_VOID )
   {
      JS_AddRoot( cx, &scopeVal_ );
      JS_AddRoot( cx, &onComplete_ );
      JS_AddRoot( cx, &onCancel_ );
   }

   ~flashPollHandler_t( void )
   {
      JS_RemoveRoot( execContext_, &scopeVal_ );
      JS_RemoveRoot( execContext_, &onComplete_ );
      JS_RemoveRoot( execContext_, &onCancel_ );
   }

   virtual void onDataAvail( void );     // POLLIN

   JSObject   *scope_ ;
   jsval       scopeVal_ ;
   jsval       onComplete_ ;
   jsval       onCancel_ ;
};


void flashPollHandler_t :: onDataAvail( void )
{
   flashThread_t :: event_e event ;
   if( sizeof( event ) == read( getFd(), &event, sizeof( event ) ) )
   {
      if( flashThread_t::complete_e == event )
      {
         if( JSVAL_VOID != onComplete_ )
            queueUnrootedSource( scope_, onComplete_, "onFlashComplete" );
      }
      else if( flashThread_t::cancel_e == event )
      {
         if( JSVAL_VOID != onCancel_ )
            queueUnrootedSource( scope_, onCancel_, "onFlashCancel" );
      }
      else
         JS_ReportError( execContext_, "Invalid flashThread event:%u\n", event );
   }
}


struct flashPrivate_t {
   flashPrivate_t( JSContext    *cx,
                   JSObject     *obj,
                   unsigned long cacheHandle,
                   void const   *data,
                   unsigned long bytes,
                   unsigned      x,
                   unsigned      y,
                   unsigned      w,
                   unsigned      h,
                   unsigned      bgColor );
   ~flashPrivate_t( void );

   bool worked( void ) const { return flashData_->worked() && thread_->isAlive(); }
   inline void start( void ){ if( thread_ ) thread_->start(); }
   inline void stop( void ){ if( thread_ ) thread_->stop(); }
   inline void pause( void ){ if( thread_ ) thread_->pause(); }

   FlashInfo const &getFlashInfo( void ) const { return flashData_->flashInfo(); }
   flashPollHandler_t *getPollHandler( void ){ return pollHandler_ ; }

private:
   flashPrivate_t( flashPrivate_t const & ); // no copies
   JSContext *const    cx_ ;
   JSObject  *const    obj_ ;
   unsigned long const cacheHandle_ ;
   parsedFlash_t      *flashData_ ;
   unsigned      const x_ ;
   unsigned      const y_ ;
   unsigned      const w_ ;
   unsigned      const h_ ;
   unsigned      const bgColor_ ;
   flashThread_t      *thread_ ;
   flashPollHandler_t *pollHandler_ ;
};

flashPrivate_t :: flashPrivate_t
   ( JSContext    *cx,
     JSObject     *obj,
     unsigned long cacheHandle,
     void const   *data,
     unsigned long bytes,
     unsigned      x,
     unsigned      y,
     unsigned      w,
     unsigned      h,
     unsigned      bgColor )
   : cx_( cx )
   , obj_( obj )
   , cacheHandle_( cacheHandle )
   , flashData_( new parsedFlash_t( data, bytes ) )
   , x_( x )
   , y_( y )
   , w_( w )
   , h_( h )
   , bgColor_( bgColor )
   , thread_( 0 )
   , pollHandler_( 0 )
{
   if( flashData_->worked() )
   {
      thread_ = new flashThread_t( *flashData_, x_, y_, w_, h_, bgColor_, false );
      if( ( 0 != thread_ ) && thread_->isAlive() )
      {
         pollHandler_ = new flashPollHandler_t( cx, obj, thread_->eventReadFd(), pollHandlers_ );
         pollHandler_->setMask( POLLIN );
         pollHandlers_.add( *pollHandler_ );
      }
   }
   getCurlCache().addRef( cacheHandle_ );
}

flashPrivate_t :: ~flashPrivate_t( void )
{
   if( thread_ )
   {
      delete thread_ ;
      thread_ = 0 ;
   }

   if( pollHandler_ )
      delete pollHandler_ ;

   delete flashData_ ;
   getCurlCache().closeHandle( cacheHandle_ );
}

static JSBool
jsFlashPlay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   flashPrivate_t * const priv = (flashPrivate_t *)JS_GetInstancePrivate( cx, obj, &jsFlashClass_, NULL );
   if( ( 0 != priv ) && priv->worked() )
   {
      flashPollHandler_t *const handler = priv->getPollHandler();
      if( handler )
      {
         handler->onComplete_ = JSVAL_VOID ;
         handler->onCancel_ = JSVAL_VOID ;
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
               handler->onComplete_ = val ;
            } // have onComplete handler
            
            if( JS_GetProperty( cx, rhObj, "onCancel", &val ) 
                &&
                JSVAL_IS_STRING( val ) )
            {
               handler->onCancel_ = val ;
            } // have onComplete handler
   
         } // have right-hand object
      }

      priv->start();
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "flash movie unloaded\n" );

   return JS_TRUE ;
}

static JSBool
jsFlashStop( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   flashPrivate_t * const privData = (flashPrivate_t *)JS_GetInstancePrivate( cx, obj, &jsFlashClass_, NULL );
   
   if( ( 0 != privData ) && privData->worked() )
   {
      privData->stop();
   }
   else
      JS_ReportError( cx, "movie is already stopped" );

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSBool
jsFlashRelease( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( obj )
   {
      flashPrivate_t * const privData = (flashPrivate_t *)JS_GetInstancePrivate( cx, obj, &jsFlashClass_, NULL );
      
      if( 0 != privData )
      {
         JS_SetPrivate( cx, obj, 0 );
         JS_DefineProperty( cx,
                            obj,
                            "isLoaded",
                            JSVAL_FALSE,
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         delete privData ;
         *rval = JSVAL_TRUE ;

      }
   }
   
   return JS_TRUE ;
}

static JSFunctionSpec flashMovieMethods_[] = {
    {"play",         jsFlashPlay,           0 },
    {"stop",         jsFlashStop,           0 },
    {"release",      jsFlashRelease,        0 },
    {0}
};

enum jsFlash_tinyid {
   FLASHMOVIE_ISLOADED, 
   FLASHMOVIE_WORKED, 
   FLASHMOVIE_URL, 
   FLASHMOVIE_HTTPCODE, 
   FLASHMOVIE_FILETIME, 
   FLASHMOVIE_MIMETYPE,
   FLASHMOVIE_PARAMS,
   FLASHMOVIE_NUMFRAMES,
   FLASHMOVIE_FRAMERATE,
   FLASHMOVIE_WIDTH,
   FLASHMOVIE_HEIGHT
};

static void jsFlashFinalize(JSContext *cx, JSObject *obj);

JSClass jsFlashClass_ = {
  "flashMovie",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsFlashFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec flashMovieProperties_[] = {
  {"isLoaded",       FLASHMOVIE_ISLOADED,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"numFrames",      FLASHMOVIE_NUMFRAMES,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"frameRate",      FLASHMOVIE_FRAMERATE,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"width",          FLASHMOVIE_WIDTH,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"height",         FLASHMOVIE_HEIGHT,      JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static char const * const cObjectTypes[] = {
   "ShapeType",
   "TextType",
   "FontType",
   "SoundType",
   "BitmapType",
   "SpriteType",
   "ButtonType"
};

static void flashMovieOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   fbDevice_t &fb = getFB();
   unsigned x = 0, y = 0, width = fb.getWidth(), height = fb.getHeight(), bgColor = 0 ;
   jsval val ;
   if( JS_GetProperty( req.cx_, req.lhObj_, "x", &val ) && JSVAL_IS_INT( val ) )
      x = JSVAL_TO_INT( val );
   if( JS_GetProperty( req.cx_, req.lhObj_, "y", &val ) && JSVAL_IS_INT( val ) )
      y = JSVAL_TO_INT( val );
   if( JS_GetProperty( req.cx_, req.lhObj_, "width", &val ) && JSVAL_IS_INT( val ) )
      width = JSVAL_TO_INT( val );
   if( JS_GetProperty( req.cx_, req.lhObj_, "height", &val ) && JSVAL_IS_INT( val ) )
      height = JSVAL_TO_INT( val );
   if( JS_GetProperty( req.cx_, req.lhObj_, "bgColor", &val ) && JSVAL_IS_INT( val ) )
      bgColor = JSVAL_TO_INT( val );
         
   flashPrivate_t * const privData = new flashPrivate_t( req.cx_, 
                                                         req.lhObj_, 
                                                         req.handle_, 
                                                         data, 
                                                         size,
                                                         x, y, width, height, bgColor ); // placement new
   JS_SetPrivate( req.cx_, req.lhObj_, privData );

   if( privData->worked() )
   {
      FlashInfo const &fi = privData->getFlashInfo();
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "numFrames",
                         INT_TO_JSVAL( fi.frameCount ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "frameRate",
                         INT_TO_JSVAL( fi.frameRate ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "flashWidth",
                         INT_TO_JSVAL( fi.frameWidth ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_DefineProperty( req.cx_, 
                         req.lhObj_, 
                         "flashHeight",
                         INT_TO_JSVAL( fi.frameHeight ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
   }
   else
      JS_ReportError( req.cx_, "parsing flash file\n" );
}


static void jsFlashFinalize(JSContext *cx, JSObject *obj)
{
   if( obj )
   {
      flashPrivate_t * const privData = (flashPrivate_t *)JS_GetInstancePrivate( cx, obj, &jsFlashClass_, NULL );
      if( 0 != privData )
      {
         JS_SetPrivate( cx, obj, 0 );
         delete privData ;
      }
   }
}

static JSBool flashMovie( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsFlashClass_, JSVAL_TO_OBJECT(flashProto_), obj );
      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         JS_SetPrivate( cx, thisObj, 0 );
         JSObject *rhObj = JSVAL_TO_OBJECT( argv[0] );

         fbDevice_t &fb = getFB();
         unsigned xPos = 0, 
                  yPos = 0, 
                  width = fb.getWidth(), 
                  height = fb.getHeight(), 
                  bgColor = 0 ;

         jsval val ;
         if( JS_GetProperty( cx, rhObj, "x", &val ) 
             &&
             JSVAL_IS_INT( val ) )
         {
            xPos = JSVAL_TO_INT( val );
         } // have x position
         else
            xPos = 0 ;
         JS_DefineProperty( cx, thisObj, "x", INT_TO_JSVAL( xPos ), 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
         
         if( JS_GetProperty( cx, rhObj, "y", &val ) 
             &&
             JSVAL_IS_INT( val ) )
         {
            yPos = JSVAL_TO_INT( val );
         } // have y position
         else
            yPos = 0 ;
         JS_DefineProperty( cx, thisObj, "y", INT_TO_JSVAL( yPos ), 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
         
         if( JS_GetProperty( cx, rhObj, "width", &val ) 
             &&
             JSVAL_IS_INT( val ) )
         {
            width = JSVAL_TO_INT( val );
         } // have width
         else
            width = fb.getWidth();
         JS_DefineProperty( cx, thisObj, "width", INT_TO_JSVAL( width ), 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );

         if( JS_GetProperty( cx, rhObj, "height", &val ) 
             &&
             JSVAL_IS_INT( val ) )
         {
            height = JSVAL_TO_INT( val );
         } // have height
         else
            height = fb.getHeight();
         JS_DefineProperty( cx, thisObj, "height", INT_TO_JSVAL( height ), 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );

         if( JS_GetProperty( cx, rhObj, "bgColor", &val ) 
             &&
             JSVAL_IS_INT( val ) )
         {
            bgColor = JSVAL_TO_INT( val );
         } // have bgColor
         else
            bgColor = 0 ; // black
         JS_DefineProperty( cx, thisObj, "bgColor", INT_TO_JSVAL( bgColor ), 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );

         if( queueCurlRequest( thisObj, argv[0], cx, flashMovieOnComplete ) )
         {
            return JS_TRUE ;
         }
         else
         {
            JS_ReportError( cx, "Error queueing curlRequest" );
         }
      }
      else
         JS_ReportError( cx, "Error allocating flashMovie" );
   }
   else
      JS_ReportError( cx, "Usage: new flashMovie( {url:\"something\"} );" );
      
   return JS_FALSE ;

}

bool initJSFlash( JSContext *cx, JSObject *glob )
{
   JS_AddRoot( cx, &flashProto_ );

   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsFlashClass_,
                                  flashMovie, 1,
                                  flashMovieProperties_, 
                                  flashMovieMethods_,
                                  0, 0 );
   if( rval )
   {
      flashProto_ = OBJECT_TO_JSVAL( rval );
   }
   else
      JS_ReportError( cx, "initializing flashMovie class\n" );
   glob_ = glob ;
   return ( 0 != rval );
}

