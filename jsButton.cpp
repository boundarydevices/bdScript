/*
 * Module jsButton.cpp
 *
 * This module defines the Javascript button initialization
 * and support routines.
 *
 *
 * Change History : 
 *
 * $Log: jsButton.cpp,v $
 * Revision 1.4  2002-11-25 00:10:59  ericn
 * -modified to break through with sound effects
 *
 * Revision 1.3  2002/11/24 19:07:48  ericn
 * -added release sound
 *
 * Revision 1.2  2002/11/23 16:14:10  ericn
 * -added touch and release sounds, alpha support
 *
 * Revision 1.1  2002/11/21 14:09:52  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "jsButton.h"
#include "fbDev.h"
#include "hexDump.h"
#include "js/jscntxt.h"
#include "curlThread.h"
#include "jsGlobals.h"
#include "box.h"
#include "zOrder.h"
#include <assert.h>
#include "codeQueue.h"
#include "jsImage.h"
#include "audioQueue.h"

typedef struct buttonData_t {
   box_t          *box_ ;
   JSObject       *jsObj_ ;
   JSContext      *cx_ ;
   unsigned short *img_ ;
   unsigned char  *imgAlpha_ ;
   unsigned short  imgWidth_ ;
   unsigned short  imgHeight_ ;
   unsigned short *moveImg_ ;
   unsigned char  *moveImgAlpha_ ;
   unsigned short  moveImgWidth_ ;
   unsigned short  moveImgHeight_ ;
   unsigned short *touchImg_ ;
   unsigned char  *touchImgAlpha_ ;
   unsigned short  touchImgWidth_ ;
   unsigned short  touchImgHeight_ ;
   unsigned char  *touchSoundData_ ;
   unsigned long   touchSoundLength_ ;
   unsigned char  *releaseSoundData_ ;
   unsigned long   releaseSoundLength_ ;
};

void jsButtonFinalize(JSContext *cx, JSObject *obj)
{
   printf( "finalizing button\n" );
   buttonData_t *button = (buttonData_t *)JS_GetPrivate( cx, obj );
   if( button )
   {
      printf( "button at %p\n", button );
      JS_SetPrivate( cx, obj, 0 );

      if( button->box_ )
      {
         printf( "box at %p\n", button->box_ );
         assert( button == (buttonData_t *)button->box_->objectData_ );
         getZMap().removeBox( *button->box_ );
         destroyBox( button->box_ );
         printf( "done destroying box\n" );
         button->box_ = 0 ;
      }
      delete button ;
   } // have button data
   else
      printf( "no button data\n" );
}

static JSFunctionSpec buttonMethods_[] = {
    {0}
};

enum jsImage_tinyId {
   BUTTON_ISLOADED,
   BUTTON_X, 
   BUTTON_Y, 
   BUTTON_WIDTH, 
   BUTTON_HEIGHT, 
   BUTTON_IMAGE,
   BUTTON_TOUCHIMAGE,
   BUTTON_TOUCHSOUND,
   BUTTON_RELEASESOUND,
   BUTTON_TOUCHCODE,
   BUTTON_MOVECODE,
   BUTTON_RELEASECODE,
};


JSClass jsButtonClass_ = {
  "button",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsButtonFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec buttonProperties_[] = {
  {"isLoaded",          BUTTON_ISLOADED,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"x",                 BUTTON_X,               JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"y",                 BUTTON_Y,               JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"width",             BUTTON_WIDTH,           JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"height",            BUTTON_HEIGHT,          JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"image",             BUTTON_IMAGE,           JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"touchImage",        BUTTON_TOUCHIMAGE,      JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"touchSound",        BUTTON_TOUCHSOUND,      JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"releaseSound",      BUTTON_RELEASESOUND,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"onTouch",           BUTTON_TOUCHCODE,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"onMove",            BUTTON_MOVECODE,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"onRelease",         BUTTON_RELEASECODE,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static void doit( box_t         &box, 
                  unsigned short x, 
                  unsigned short y,
                  touchHandler_t defHandler,
                  char const    *method )
{
   buttonData_t *const button = (buttonData_t *)box.objectData_ ;
   assert( 0 != button );
   assert( button->box_ == &box );
   assert( JS_GetPrivate( button->cx_, button->jsObj_ ) == button );

   defHandler( box, x, y );
   
   jsval jsv ;
   if( JS_GetProperty( button->cx_, button->jsObj_, method, &jsv ) && JSVAL_IS_STRING( jsv ) )
   {
      if( !queueSource( button->jsObj_, JS_GetStringBytes( JSVAL_TO_STRING(jsv) ), "buttonTouch" ) )
         JS_ReportError( button->cx_, "Error queueing button handler" );
   }
}

static void display( unsigned short  xLeft,
                     unsigned short  yTop,
                     unsigned short *img,
                     unsigned char  *alpha,
                     unsigned short  imgWidth,
                     unsigned short  imgHeight )
{
   if( 0 != img )
   {
      getFB().render( xLeft, yTop, imgWidth, imgHeight, img, alpha );
   }
}


static void buttonTouch( box_t         &box, 
                         unsigned short x, 
                         unsigned short y )
{
   buttonData_t *const button = (buttonData_t *)box.objectData_ ;
   assert( 0 != button );
   assert( button->box_ == &box );

   display( box.xLeft_, box.yTop_, button->touchImg_, button->touchImgAlpha_, button->touchImgWidth_, button->touchImgHeight_ );

   if( ( 0 != button->touchSoundData_ ) && ( 0 != button->touchSoundLength_ ) )
   {
      audioQueue_t &q = getAudioQueue();
      unsigned numCancelled ;
      q.clear( numCancelled );
      q.insert( button->jsObj_, button->touchSoundData_, button->touchSoundLength_, "", "" );
   }

   doit( box, x, y, defaultTouch, "onTouch" );
}

static void buttonMove( box_t         &box, 
                        unsigned short x, 
                        unsigned short y )
{
   buttonData_t *const button = (buttonData_t *)box.objectData_ ;
   assert( 0 != button );
   assert( button->box_ == &box );
   display( box.xLeft_, box.yTop_, button->moveImg_, button->moveImgAlpha_, button->moveImgWidth_, button->moveImgHeight_ );
   doit( box, x, y, defaultTouchMove, "onMove" );
}

static void buttonRelease( box_t         &box, 
                           unsigned short x, 
                           unsigned short y )
{
   buttonData_t *const button = (buttonData_t *)box.objectData_ ;
   assert( 0 != button );
   assert( button->box_ == &box );
   
   display( box.xLeft_, box.yTop_, button->img_, button->imgAlpha_, button->imgWidth_, button->imgHeight_ );
   
   if( ( 0 != button->releaseSoundData_ ) && ( 0 != button->releaseSoundLength_ ) )
   {
      audioQueue_t &q = getAudioQueue();
      unsigned numCancelled ;
      q.clear( numCancelled );
      q.insert( button->jsObj_, button->releaseSoundData_, button->releaseSoundLength_, "", "" );
   }

   doit( box, x, y, defaultRelease, "onRelease" );
}

static JSBool button( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) 
       && 
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = js_NewObject( cx, &jsButtonClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         JS_SetPrivate( cx, thisObj, 0 );

         JS_DefineProperty( cx, thisObj, "isLoaded",
                            JSVAL_FALSE,
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JSObject *const rhObj = JSVAL_TO_OBJECT( argv[0] );
         if( rhObj )
         {
            jsval     vX ;
            jsval     vY ;
            jsval     vW ;
            jsval     vH ;
            jsval     vImage ;
            JSObject *oImage ;

            if( JS_GetProperty( cx, rhObj, "x", &vX ) && JSVAL_IS_INT( vX )
                &&
                JS_GetProperty( cx, rhObj, "y", &vY ) && JSVAL_IS_INT( vY )
                &&
                JS_GetProperty( cx, rhObj, "width", &vW ) && JSVAL_IS_INT( vW )
                &&
                JS_GetProperty( cx, rhObj, "height", &vH ) && JSVAL_IS_INT( vH )
                &&
                JS_GetProperty( cx, rhObj, "img", &vImage ) && JSVAL_IS_OBJECT( vImage ) 
                &&
                ( 0 != ( oImage = JSVAL_TO_OBJECT( vImage ) ) ) )
            {
               JS_DefineProperty( cx, thisObj, "x", vX, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "y", vY, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "width", vW, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "height", vH, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "image", vImage, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );

               jsval drawParams[2] = {
                  vX, vY
               };

               jsval drawResult ;
               jsImageDraw( cx, oImage, 2, drawParams, &drawResult );

               buttonData_t *const buttonData = new buttonData_t ;
               printf( "new buttonData %p\n", buttonData );
               memset( buttonData, 0, sizeof( *buttonData ) );
               
               fbDevice_t &fb = getFB();
               
               unsigned x = JSVAL_TO_INT( vX );
               if( x > fb.getWidth() )
                  x = fb.getWidth()-1 ;

               unsigned y = JSVAL_TO_INT( vY );
               if( y > fb.getHeight() )
                  y = fb.getHeight()-1 ;
               
               unsigned w = JSVAL_TO_INT( vW );
               unsigned right = x + w ;
               if( right > fb.getWidth() )
                  right = fb.getWidth();

               unsigned h = JSVAL_TO_INT( vH );
               unsigned bottom = y + h ;
               if( bottom > fb.getHeight() )
                  bottom = fb.getHeight();

               box_t *const box = buttonData->box_ = newBox( x, right, y, bottom, buttonData );
               printf( "new box %p/%p/%p, %u-%u, %u-%u\n", box, box->objectData_, buttonData->box_, x, right, y, bottom );
               buttonData->cx_    = cx ;
               buttonData->jsObj_ = thisObj ;
               box->onTouch_     = buttonTouch ;
               box->onTouchMove_ = buttonMove ;
               box->onRelease_   = buttonRelease ;

               JS_SetPrivate( cx, thisObj, buttonData );

               jsval     jsv ;
               JSObject *jsO ;
               int       iVal ;
               if( JS_GetProperty( cx, oImage, "width", &jsv ) && JSVAL_IS_INT( jsv ) )
               {
                  buttonData->imgWidth_ = JSVAL_TO_INT( jsv );
                  if( JS_GetProperty( cx, oImage, "height", &jsv ) && JSVAL_IS_INT( jsv ) )
                  {
                     buttonData->imgHeight_ = JSVAL_TO_INT( jsv );
                     if( JS_GetProperty( cx, oImage, "pixBuf", &jsv ) && JSVAL_IS_STRING( jsv ) )
                     {
                        buttonData->img_ = (unsigned short *)JS_GetStringBytes( JSVAL_TO_STRING(jsv) );
                        if( JS_GetProperty( cx, oImage, "alpha", &jsv ) && JSVAL_IS_STRING( jsv ) )
                           buttonData->imgAlpha_ = (unsigned char *)JS_GetStringBytes( JSVAL_TO_STRING(jsv) );
                     }
                  }
               }
               
               if( JS_GetProperty( cx, rhObj, "touchImg", &jsv ) && JSVAL_IS_OBJECT( jsv ) && ( 0 != ( oImage = JSVAL_TO_OBJECT( jsv ) ) ) )
               {
                  JS_DefineProperty( cx, thisObj, "touchImage", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  if( JS_GetProperty( cx, oImage, "width", &jsv ) && JSVAL_IS_INT( jsv ) )
                  {
                     buttonData->touchImgWidth_ = JSVAL_TO_INT( jsv );
                     if( JS_GetProperty( cx, oImage, "height", &jsv ) && JSVAL_IS_INT( jsv ) )
                     {
                        buttonData->touchImgHeight_ = JSVAL_TO_INT( jsv );
                        if( JS_GetProperty( cx, oImage, "pixBuf", &jsv ) && JSVAL_IS_STRING( jsv ) )
                        {
                           buttonData->touchImg_ = (unsigned short *)JS_GetStringBytes( JSVAL_TO_STRING(jsv) );
                           if( JS_GetProperty( cx, oImage, "alpha", &jsv ) && JSVAL_IS_STRING( jsv ) )
                              buttonData->touchImgAlpha_ = (unsigned char *)JS_GetStringBytes( JSVAL_TO_STRING(jsv) );
                        }
                     }
                  }
               }
               
               if( JS_GetProperty( cx, rhObj, "touchSound", &jsv ) && JSVAL_IS_OBJECT( jsv ) && ( 0 != ( jsO = JSVAL_TO_OBJECT( jsv ) ) ) )
               {
                  JS_DefineProperty( cx, thisObj, "touchSound", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  if( JS_GetProperty( cx, jsO, "data", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  {
                     JSString *sVal = JSVAL_TO_STRING(jsv);
                     buttonData->touchSoundData_ = (unsigned char *)JS_GetStringBytes( sVal );
                     buttonData->touchSoundLength_ = JS_GetStringLength( sVal );
                  }
               }
               if( JS_GetProperty( cx, rhObj, "releaseSound", &jsv ) && JSVAL_IS_OBJECT( jsv ) && ( 0 != ( jsO = JSVAL_TO_OBJECT( jsv ) ) ) )
               {
                  JS_DefineProperty( cx, thisObj, "releaseSound", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  if( JS_GetProperty( cx, jsO, "data", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  {
                     JSString *sVal = JSVAL_TO_STRING(jsv);
                     buttonData->releaseSoundData_ = (unsigned char *)JS_GetStringBytes( sVal );
                     buttonData->releaseSoundLength_ = JS_GetStringLength( sVal );
                  }
               }

               if( JS_GetProperty( cx, rhObj, "onTouch", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  JS_DefineProperty( cx, thisObj, "onTouch", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               if( JS_GetProperty( cx, rhObj, "onMove", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  JS_DefineProperty( cx, thisObj, "onMove", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               if( JS_GetProperty( cx, rhObj, "onRelease", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  JS_DefineProperty( cx, thisObj, "onRelease", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               
               getZMap().addBox( *box );
            }
            else
               JS_ReportError( cx, "Missing required params" );
         }
         else
            JS_ReportError( cx, "Error getting button initializer" );
      }
      else
         JS_ReportError( cx, "Error allocating button" );
   }
   else
      JS_ReportError( cx, "Usage : new button( { url:\"something\" } );" );
      
   return JS_TRUE ;

}

bool initJSButton( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsButtonClass_,
                                  button, 1,
                                  buttonProperties_, 
                                  buttonMethods_,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      return false ;

}


