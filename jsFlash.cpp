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
 * Revision 1.3  2003-08-06 13:23:56  ericn
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
#include "flash/flash.h"
#include "flash/swf.h"
#include "flash/movie.h"
#include "ccActiveURL.h"
#include "fbDev.h"
#include <pthread.h>
#include <linux/timer.h>
#include "semClasses.h"
#include "codeQueue.h"
#include "flash/sound.h"
#include "audioQueue.h"

extern JSClass jsFlashClass_ ;

class flashPrivate_t {
public:
   flashPrivate_t( JSContext    *cx,
                   JSObject     *obj,
                   unsigned long cacheHandle,
                   FlashHandle   flashHandle );
   ~flashPrivate_t( void );

   inline bool isRunning( void ){ return (pthread_t)-1 != threadHandle_ ; }
   void stop( bool runHandlers_ );
   bool start( jsval onComplete,
               jsval onCancel,
               unsigned xPos,
               unsigned yPos,
               unsigned width,
               unsigned height,
               unsigned bgColor );

   JSContext    *cx_ ;
   JSObject     *obj_ ;
   unsigned long cacheHandle_ ;
   FlashHandle   flashHandle_ ;
   pthread_t     threadHandle_ ;
   mutex_t       dieMutex_ ;
   condition_t   dieCondition_ ;
   jsval         onComplete_ ;
   jsval         onCancel_ ;
   unsigned      xPos_ ;
   unsigned      yPos_ ;
   unsigned      width_ ;
   unsigned      height_ ;
   unsigned      bgColor_ ;
   bool volatile cancelled_ ;
   unsigned volatile numSoundsPlaying_ ;
private:
   flashPrivate_t( flashPrivate_t const & );
};

static void
getUrl(char *url, char *target, void *client_data)
{
	printf("GetURL : %s\n", url);
}

static void
getSwf(char *url, int level, void *client_data)
{
	printf("GetSwf : %s\n", url);
}

class flashSoundMixer : public SoundMixer {
public:
	flashSoundMixer( flashPrivate_t &parent );
	~flashSoundMixer();

	void		 startSound(Sound *sound);	// Register a sound to be played
	void		 stopSounds();		// Stop every current sounds in the instance
        flashPrivate_t  &parent_ ;
};

flashSoundMixer :: flashSoundMixer( flashPrivate_t &parent )
   : SoundMixer( "" ), // keep it happy
     parent_( parent )
{
}

flashSoundMixer :: ~flashSoundMixer()
{
}

#include "hexDump.h"

static void flashSoundComplete( void *param )
{
   printf( "flashSoundComplete:%p\n", param );
}

void flashSoundMixer :: startSound(Sound *sound)
{
   getAudioQueue().queuePlayback( (unsigned char *)sound->getSamples(), 
                                  sound->getSampleSize()*sound->getNbSamples(),
                                  sound,
                                  flashSoundComplete );
   mutexLock_t lock( parent_.dieMutex_ );
   ++parent_.numSoundsPlaying_ ;
}

void flashSoundMixer :: stopSounds()
{
   unsigned numCancelled ;
   getAudioQueue().clear( numCancelled );
   mutexLock_t lock( parent_.dieMutex_ );
   --parent_.numSoundsPlaying_ ;
}

static void *flashThreadRoutine( void *param )
{
   flashPrivate_t &priv = *( flashPrivate_t * )param ;

   bool completed = false ;
   
   FlashMovie *fh = (FlashMovie *)priv.flashHandle_ ;

   fh->sm = new flashSoundMixer( priv );

//   FlashSoundInit( priv.flashHandle_, "/dev/dsp");
   
   fbDevice_t &fb = getFB();
   FlashDisplay display ;

   display.width  = ( 0 == priv.width_ ) ? fb.getWidth() : priv.width_ ;
   if( display.width > fb.getWidth() )
      display.width = fb.getWidth();

   display.height = ( 0 == priv.height_ ) ? fb.getHeight() : priv.height_ ;
   if( display.height > fb.getHeight() )
      display.height = fb.getHeight();

   unsigned short * const pixels = new unsigned short [ display.width*display.height ];
   display.pixels = pixels ;
   display.bpl    = display.width*sizeof(pixels[0]);
   unsigned const videoBytes = display.width*display.height*sizeof(pixels[0]);
   unsigned short const bg = fb.get16( ( priv.bgColor_ >> 16 ),
                                       ( priv.bgColor_ >> 8 ) & 0xFF,
                                       ( priv.bgColor_ & 0xFF ) );
   // set top row
   for( unsigned i = 0 ; i < display.width ; i++ )
      pixels[i] = bg ;
   // set remaining rows 
   for( unsigned i = 1 ; i < display.height ; i++ )
      memcpy( pixels+(i*display.width), pixels, display.bpl );
   unsigned short * const fbMem = (unsigned short *)fb.getMem() + priv.xPos_ + ( priv.yPos_ * fb.getWidth() );
   unsigned const fbStride = fb.getWidth()*sizeof(pixels[0]);

   display.depth  = sizeof( pixels[0]);
   display.bpp    = sizeof( pixels[0]);
   display.flash_refresh = 0 ;
   display.clip_x = 0 ;
   display.clip_y = 0 ;
   display.clip_width = display.width ;
   display.clip_height = display.height ;

   FlashGraphicInit( priv.flashHandle_, &display );

   FlashSetGetUrlMethod( priv.flashHandle_, getUrl, 0);
   FlashSetGetSwfMethod( priv.flashHandle_, getSwf, 0);

   do {
      mutexLock_t lock( priv.dieMutex_ );
      if( lock.worked() )
      {
         if( priv.dieCondition_.wait( lock, 100 ) )
         {
            break;
         }
         else
         {
             struct timeval wd ;
             long wakeUp = FlashExec( priv.flashHandle_, FLASH_WAKEUP, 0, &wd);
             
             if( display.flash_refresh )
             {
                char *pixMem = (char *)fbMem ;
                char *flashMem = (char *)display.pixels ;
                for( unsigned i = 0 ; i < display.height ; i++, pixMem += fbStride, flashMem += display.bpl )
                {
                   memcpy( pixMem, flashMem, display.bpl );
                }
             }

             if( wakeUp )
             {
                struct timeval now ;
                gettimeofday(&now,0);

                if( wd.tv_sec >= now.tv_sec )
                {
                   struct timespec delay ;
                   delay.tv_sec = wd.tv_sec - now.tv_sec ;
                   if( wd.tv_usec < now.tv_usec )
                   {
                      if( 0 < delay.tv_sec )
                      {
                         --delay.tv_sec ;
                         delay.tv_nsec = ( 1000000 + wd.tv_usec - now.tv_usec ) * 1000 ;
                      }
                      else
                      {
                         delay.tv_nsec = delay.tv_nsec = 0 ;
                      } // too late
                   } // borrow
                   else
                      delay.tv_nsec = ( wd.tv_usec - now.tv_usec ) * 1000 ;
                   
                   if( delay.tv_sec || delay.tv_nsec )
                   {
                      struct timespec remaining ;
//                      nanosleep( &delay, &remaining );
                   }
                } // not WAY too late
             }
             else
                completed = true ;
         }
      }
      else
      {
         fprintf( stderr, "flashLockErr\n" ); fflush( stderr );
         break;
      }
   } while( !( completed || priv.cancelled_ ) );

   if( 0 != priv.numSoundsPlaying_ )
   {
      unsigned numCancelled;
      getAudioQueue().clear( numCancelled );
      priv.numSoundsPlaying_ = 0 ;
   }

   jsval const handler = completed ? priv.onComplete_ : priv.onCancel_ ;
   if( JSVAL_VOID != handler )
   {
      if( !queueSource( priv.obj_, handler, "flashHandler" ) )
      {
         fprintf( stderr, "Error calling flashMovie.handler()\n" ); fflush( stderr );
      }
   }
   else
   {
   }

   delete [] pixels ;
}

flashPrivate_t::flashPrivate_t
   ( JSContext    *cx,
     JSObject     *obj,
     unsigned long cacheHandle,
     FlashHandle   flashHandle )
   : cx_( cx ),
     obj_( obj ),
     cacheHandle_( cacheHandle ), 
     flashHandle_( flashHandle ), 
     threadHandle_( (pthread_t)-1 ),
     dieMutex_(),
     dieCondition_(),
     onComplete_( JSVAL_VOID ),
     onCancel_( JSVAL_VOID ),
     xPos_( 0 ),
     yPos_( 0 ),
     width_( 0 ),
     height_( 0 ),
     bgColor_( 0 ),
     cancelled_( false ),
     numSoundsPlaying_( 0 )
{
   getCurlCache().addRef( cacheHandle_ );
   JS_AddRoot( cx, &onComplete_ );
   JS_AddRoot( cx, &onCancel_ );
}

flashPrivate_t::~flashPrivate_t( void )
{
   if( isRunning() )
      stop( false );
   JS_RemoveRoot( cx_, &onComplete_ );
   JS_RemoveRoot( cx_, &onCancel_ );
   getCurlCache().closeHandle( cacheHandle_ );
   if( 0 != flashHandle_ )
      FlashClose( flashHandle_ );
}

void flashPrivate_t::stop( bool runHandlers )
{ 
   if( isRunning() )
   {
      cancelled_ = true ;

      {
         mutexLock_t lock( dieMutex_ );
         if( !lock.worked() ){ fprintf( stderr, "error locking mutex!\n" ); fflush( stderr ); }
         if( !runHandlers )
         {
            onComplete_ = JSVAL_VOID ;
            onCancel_   = JSVAL_VOID ;
         }

         dieCondition_.signal(); 
      }
      void *exitStat ; 
      pthread_join( threadHandle_, &exitStat ); 
      threadHandle_ = (pthread_t)-1 ; 
   }
}

bool flashPrivate_t::start
   ( jsval onComplete,
     jsval onCancel,
     unsigned xPos,
     unsigned yPos,
     unsigned width,
     unsigned height,
     unsigned bgColor )
{
   onComplete_ = onComplete ;
   onCancel_ = onCancel ;
   xPos_ = xPos ;
   yPos_ = yPos ;
   width_ = width ;
   height_ = height ;
   bgColor_ = bgColor ;

   if( !isRunning() )
   {
      cancelled_ = false ;
      int create = pthread_create( &threadHandle_, 0, flashThreadRoutine, this );
      return 0 == create ;
   }
   else
      return false ;
}

static JSBool
jsFlashPlay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   flashPrivate_t * const privData = (flashPrivate_t *)JS_GetInstancePrivate( cx, obj, &jsFlashClass_, NULL );
   
   if( 0 != privData )
   {
      if( !privData->isRunning() )
      {
         jsval onComplete = JSVAL_VOID ;
         jsval onCancel = JSVAL_VOID ;
         
         unsigned xPos = 0 ;
         unsigned yPos = 0 ;
         unsigned width =  0 ;
         unsigned height = 0 ;
         unsigned bgColor = 0 ;

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
            
            if( JS_GetProperty( cx, rhObj, "width", &val ) 
                &&
                JSVAL_IS_INT( val ) )
            {
               width = JSVAL_TO_INT( val );
            } // have width
            
            if( JS_GetProperty( cx, rhObj, "height", &val ) 
                &&
                JSVAL_IS_INT( val ) )
            {
               height = JSVAL_TO_INT( val );
            } // have height

            if( JS_GetProperty( cx, rhObj, "bgColor", &val ) 
                &&
                JSVAL_IS_INT( val ) )
            {
               bgColor = JSVAL_TO_INT( val );
            } // have bgColor
            
         } // have right-hand object

         if( privData->start( onComplete, onCancel, xPos, yPos, width, height, bgColor ) )
            *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "movie is already playing" );
   }
   else
      JS_ReportError( cx, "flash movie unloaded\n" );

   return JS_TRUE ;
}

static JSBool
jsFlashStop( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   flashPrivate_t * const privData = (flashPrivate_t *)JS_GetInstancePrivate( cx, obj, &jsFlashClass_, NULL );
   
   if( 0 != privData )
   {
      if( privData->isRunning() )
      {
         privData->stop( true );
      }
      else
         JS_ReportError( cx, "movie is already stopped" );
   }
   else
      JS_ReportError( cx, "flash movie unloaded\n" );

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSBool
jsFlashRelease( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
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
         privData->~flashPrivate_t();

         JS_free( cx, privData );
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
   FlashHandle flashHandle = FlashNew();
   if( 0 != flashHandle )
   {
      int status ;

      // Load level 0 movie
      do {
         status = FlashParse(flashHandle, 0, (char *)data, size );
      } while( status & FLASH_PARSE_NEED_DATA );
      
      if( FLASH_PARSE_START & status )
      {
         struct FlashInfo fi;
         FlashGetInfo( flashHandle, &fi );

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
                            "width",
                            INT_TO_JSVAL( fi.frameWidth ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JS_DefineProperty( req.cx_, 
                            req.lhObj_, 
                            "height",
                            INT_TO_JSVAL( fi.frameHeight ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
      }
      else
         JS_ReportError( req.cx_, "parsing flash file\n" );

   }
   else
      JS_ReportError( req.cx_, "allocating flash handle\n" );

   void * const privMem = JS_malloc( req.cx_, sizeof( flashPrivate_t ) );
   flashPrivate_t * const privData = new (privMem)flashPrivate_t( req.cx_, req.lhObj_, req.handle_, flashHandle ); // placement new
   JS_SetPrivate( req.cx_, req.lhObj_, privData );
}


static void jsFlashFinalize(JSContext *cx, JSObject *obj)
{
   if( obj )
   {
      flashPrivate_t * const privData = (flashPrivate_t *)JS_GetInstancePrivate( cx, obj, &jsFlashClass_, NULL );
      if( 0 != privData )
      {
         JS_SetPrivate( cx, obj, 0 );
         privData->~flashPrivate_t();
         JS_free( cx, privData );
      }
   }
}

static JSBool flashMovie( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsFlashClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         JS_SetPrivate( cx, thisObj, 0 );

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
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsFlashClass_,
                                  flashMovie, 1,
                                  flashMovieProperties_, 
                                  flashMovieMethods_,
                                  0, 0 );
   return ( 0 != rval );
}

