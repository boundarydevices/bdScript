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
 * Revision 1.1  2002-11-21 14:09:52  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "jsButton.h"
#include "fbDev.h"
#include "hexDump.h"
#include "js/jscntxt.h"
#include "imgGIF.h"
#include "imgPNG.h"
#include "imgJPEG.h"
#include "curlThread.h"
#include "jsGlobals.h"
#include "box.h"
#include "zOrder.h"
#include <assert.h>
#include "codeQueue.h"

typedef struct buttonData_t {
   enum fileFlags_e {
      haveTouchImage_   = 1,
      haveTouchSound_   = 2,
      haveReleaseSound_ = 4,
   };

   unsigned char  desiredComponents_ ;
   unsigned char  componentsPresent_ ;
   fileFlags_e    downloading_ ;
   box_t         *box_ ;
   JSObject      *jsObj_ ;
   JSContext     *cx_ ;
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
   BUTTON_IMAGEURL,
   BUTTON_TOUCHIMAGEURL,
   BUTTON_TOUCHSOUNDURL,
   BUTTON_RELEASESOUNDURL,
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
  {"imageURL",          BUTTON_IMAGEURL,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"touchImageURL",     BUTTON_TOUCHIMAGEURL,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"touchSoundURL",     BUTTON_TOUCHSOUNDURL,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"releaseSoundURL",   BUTTON_RELEASESOUNDURL, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
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

static void buttonTouch( box_t         &box, 
                         unsigned short x, 
                         unsigned short y )
{
   doit( box, x, y, defaultTouch, "onTouch" );
}

static void buttonMove( box_t         &box, 
                        unsigned short x, 
                        unsigned short y )
{
   doit( box, x, y, defaultTouchMove, "onMove" );
}

static void buttonRelease( box_t         &box, 
                           unsigned short x, 
                           unsigned short y )
{
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
            JSString *sImageURL ;

            if( JS_GetProperty( cx, rhObj, "x", &vX ) && JSVAL_IS_INT( vX )
                &&
                JS_GetProperty( cx, rhObj, "y", &vY ) && JSVAL_IS_INT( vY )
                &&
                JS_GetProperty( cx, rhObj, "width", &vW ) && JSVAL_IS_INT( vW )
                &&
                JS_GetProperty( cx, rhObj, "height", &vH ) && JSVAL_IS_INT( vH )
                &&
                JS_GetProperty( cx, rhObj, "img", &vImage ) && JSVAL_IS_STRING( vImage ) 
                &&
                ( 0 != ( sImageURL = JSVAL_TO_STRING( vImage ) ) ) )
            {
               JS_DefineProperty( cx, thisObj, "x", vX, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "y", vY, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "width", vW, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "height", vH, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "imageURL", vImage, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );

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

               jsval jsv ;
               if( JS_GetProperty( cx, rhObj, "touchImg", &jsv ) && JSVAL_IS_STRING( jsv ) )
               {
                  JS_DefineProperty( cx, thisObj, "touchImageURL", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               }
               if( JS_GetProperty( cx, rhObj, "touchSound", &jsv ) && JSVAL_IS_STRING( jsv ) )
               {
                  JS_DefineProperty( cx, thisObj, "touchSoundURL", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               }
               if( JS_GetProperty( cx, rhObj, "releaseSound", &jsv ) && JSVAL_IS_STRING( jsv ) )
               {
                  JS_DefineProperty( cx, thisObj, "releaseSoundURL", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               }
               if( JS_GetProperty( cx, rhObj, "onTouch", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  JS_DefineProperty( cx, thisObj, "onTouch", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               if( JS_GetProperty( cx, rhObj, "onMove", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  JS_DefineProperty( cx, thisObj, "onMove", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               if( JS_GetProperty( cx, rhObj, "onRelease", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  JS_DefineProperty( cx, thisObj, "onRelease", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               
               getZMap().addBox( *box );
printf( "added box %u to zMap\n", box->id_ );
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


