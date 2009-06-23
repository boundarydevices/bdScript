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
 * Revision 1.28  2009-06-09 02:52:23  tkisky
 * fix munmap size
 *
 * Revision 1.27  2009-05-12 21:46:52  valli
 *
 * updated jsButton to clear audioQueue before playing either pressSnd or
 * releaseSnd.
 *
 * Revision 1.26  2006-06-10 16:28:43  ericn
 * -allow predecoded release sounds
 *
 * Revision 1.25  2006/05/14 14:44:30  ericn
 * -allow predecoded audio through audioOutPoll
 *
 * Revision 1.24  2005/11/06 00:49:26  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.23  2003/11/25 00:13:17  ericn
 * -no need to lock from button handlers
 *
 * Revision 1.22  2003/08/26 00:33:13  ericn
 * -added deactivate() method
 *
 * Revision 1.21  2003/08/23 21:36:37  ericn
 * -added draw, allow function for callbacks
 *
 * Revision 1.20  2003/02/10 01:17:46  ericn
 * -modified to allow truncation of text
 *
 * Revision 1.19  2003/02/07 04:41:35  ericn
 * -removed audio cancel on press/release
 *
 * Revision 1.18  2003/02/07 03:01:33  ericn
 * -made freeTypeLibrary_t internal and persistent
 *
 * Revision 1.17  2003/02/01 18:14:54  ericn
 * -modified to use queuePlayback() instead of insert()
 *
 * Revision 1.16  2003/01/31 13:30:05  ericn
 * -modified to allow empty buttons
 *
 * Revision 1.15  2002/12/26 19:26:59  ericn
 * -added onMoveOff support
 *
 * Revision 1.14  2002/12/26 19:04:26  ericn
 * -lock touch flags, execute code directly from Javascript thread
 *
 * Revision 1.13  2002/12/16 14:25:41  ericn
 * -removed warning messages
 *
 * Revision 1.12  2002/12/15 20:01:44  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.11  2002/12/11 04:04:48  ericn
 * -moved buttonize code from button to fbDev
 *
 * Revision 1.10  2002/12/07 23:19:48  ericn
 * -fixed msg termination and allocation, added changeText method
 *
 * Revision 1.9  2002/12/07 21:01:37  ericn
 * -added support for text buttons
 *
 * Revision 1.8  2002/12/01 02:42:44  ericn
 * -modified to root handlers on way through queue
 *
 * Revision 1.7  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.6  2002/11/30 05:26:39  ericn
 * -removed debug msgs, added lock
 *
 * Revision 1.5  2002/11/30 00:32:18  ericn
 * -removed curlCache and curlThread modules
 *
 * Revision 1.4  2002/11/25 00:10:59  ericn
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
#include "jsGlobals.h"
#include "box.h"
#include "zOrder.h"
#include <assert.h>
#include "codeQueue.h"
#include "jsImage.h"
#include "audioQueue.h"
#include "audioOutPoll.h"
#include "ftObjs.h"

typedef struct buttonData_t {
   box_t          *box_ ;
   JSObject       *jsObj_ ;
   JSContext      *cx_ ;
   // these fields are used for image buttons
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

   // these fields are used for text buttons
   char const     *msgString_ ;
   unsigned long   bgColor_ ;
   unsigned long   textColor_ ;
   unsigned short  buttonWidth_ ;
   unsigned short  buttonHeight_ ;
   unsigned short  pointSize_ ;
   unsigned char   borderWidth_ ;
   void const     *fontData_ ;
   unsigned long   fontSize_ ;
   unsigned long   getX;
   unsigned long   getY;

   bool            touchSoundDecoded_ ;
   unsigned char  *touchSoundData_ ;
   unsigned long   touchSoundLength_ ;
   bool            releaseSoundDecoded_ ;
   unsigned char  *releaseSoundData_ ;
   unsigned long   releaseSoundLength_ ;
};

void jsButtonFinalize(JSContext *cx, JSObject *obj)
{
   buttonData_t *button = (buttonData_t *)JS_GetPrivate( cx, obj );
   if( button )
   {
      JS_SetPrivate( cx, obj, 0 );

      if( button->box_ )
      {
         assert( button == (buttonData_t *)button->box_->objectData_ );
         getZMap().removeBox( *button->box_ );
         destroyBox( button->box_ );
         button->box_ = 0 ;
      }
      if( button->msgString_ )
         delete [] (char *)button->msgString_ ;
      delete button ;
   } // have button data
//      else
//         printf( "no button data\n" );
// this seems to be normal, and unrelated to finalization of a button object
//
}

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
   BUTTON_MOVEOFFCODE,
   BUTTON_RELEASECODE,
   BUTTON_FONT,
   BUTTON_TEXT
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
  {"onMoveOff",         BUTTON_MOVEOFFCODE,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"onRelease",         BUTTON_RELEASECODE,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"font",              BUTTON_FONT,            JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"text",              BUTTON_TEXT,            JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
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

   button->getX = x;
   button->getY = y;
   defHandler( box, x, y );
   
   jsval jsv ;
   if( JS_GetProperty( button->cx_, button->jsObj_, method, &jsv ) )
   {
      JSType const type = JS_TypeOfValue( button->cx_, jsv );
      if( ( JSTYPE_STRING == type )
          ||
          ( JSTYPE_FUNCTION == type ) )
      {
         executeCode( button->jsObj_, jsv, method );
      }
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

static void drawButton( buttonData_t const &button, bool pressed )
{
   fbDevice_t &fb = getFB();
   fb.buttonize( pressed,
                 button.borderWidth_,
                 button.box_->xLeft_, button.box_->yTop_, button.box_->xRight_, button.box_->yBottom_, 
                 button.bgColor_ >> 16, button.bgColor_ >> 8, button.bgColor_ );

   unsigned short b[4] = { button.box_->xLeft_, button.box_->yTop_, button.box_->xRight_, button.box_->yBottom_ };

   if( pressed )
   {
      b[0] += button.borderWidth_ ;
      b[2] += button.borderWidth_ ;
   }

   freeTypeFont_t font( button.fontData_, button.fontSize_ );
   if( font.worked() )
   {
      unsigned const boxWidth  = b[2]-b[0];
      freeTypeString_t ftString( font, button.pointSize_, button.msgString_, strlen( button.msgString_ ),
                                 boxWidth );

      if( boxWidth > ftString.getWidth() )
      {
         unsigned const diff = boxWidth-ftString.getWidth();
         unsigned const leftMarg = diff/2 ;
         b[0] += leftMarg ;
         b[2] -= diff-leftMarg ;
      } // center horizontally

      unsigned const boxHeight = b[3]-b[1];
      if( boxHeight > ftString.getHeight() )
      {
         unsigned const diff = boxHeight-ftString.getHeight();
         unsigned const topMarg = diff/2 ;
         b[1] += topMarg ;
         b[3] -= ( diff-topMarg );
      } // center vertically

      fb.antialias( ftString.getRow(0),
                    ftString.getWidth(),
                    ftString.getHeight(),
                    b[0], b[1], b[2], b[3],
                    (unsigned char)( button.textColor_ >> 16 ),
                    (unsigned char)( button.textColor_ >> 8 ),
                    (unsigned char)( button.textColor_ ) );
   }
}

static void buttonTouch( box_t         &box, 
                         unsigned short x, 
                         unsigned short y )
{
   buttonData_t *const button = (buttonData_t *)box.objectData_ ;
   assert( 0 != button );
   assert( button->box_ == &box );

   if( 0 != button->touchImg_ )
      display( box.xLeft_, box.yTop_, button->touchImg_, button->touchImgAlpha_, button->touchImgWidth_, button->touchImgHeight_ );
   else if( 0 != button->fontData_ )
      drawButton( *button, true );

   if( ( 0 != button->touchSoundData_ ) && ( 0 != button->touchSoundLength_ ) )
   {
      audioOutPoll_t *ao ;
      if( button->touchSoundDecoded_ 
          && 
          ( 0 != ( ao = audioOutPoll_t::get() ) ) )
      {
         ao->queuePlayback( *(audioQueue_t::waveHeader_t *)button->touchSoundData_ );
      }
      else
      {
         audioQueue_t &q = getAudioQueue();
         unsigned int numCancelled = 0;
         q.clear(numCancelled);
         if( !button->touchSoundDecoded_ )
            q.queuePlayback( button->jsObj_, button->touchSoundData_, button->touchSoundLength_ );
         else
            q.queuePlayback( button->jsObj_, *(audioQueue_t::waveHeader_t *)button->touchSoundData_ );
      }
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

   if( 0 != button->moveImg_ )
      display( box.xLeft_, box.yTop_, button->moveImg_, button->moveImgAlpha_, button->moveImgWidth_, button->moveImgHeight_ );
   
   doit( box, x, y, defaultTouchMove, "onMove" );
}

static void buttonMoveOff( box_t         &box, 
                           unsigned short x, 
                           unsigned short y )
{
   buttonData_t *const button = (buttonData_t *)box.objectData_ ;
   assert( 0 != button );
   assert( button->box_ == &box );

   if( 0 != button->img_ )
      display( box.xLeft_, box.yTop_, button->img_, button->imgAlpha_, button->imgWidth_, button->imgHeight_ );
   else if( 0 != button->fontData_ )
      drawButton( *button, false );
   
   doit( box, x, y, defaultTouchMoveOff, "onMoveOff" );
}

static void buttonRelease( box_t         &box, 
                           unsigned short x, 
                           unsigned short y )
{
   buttonData_t *const button = (buttonData_t *)box.objectData_ ;
   assert( 0 != button );
   assert( button->box_ == &box );
   
   if( 0 != button->img_ )
      display( box.xLeft_, box.yTop_, button->img_, button->imgAlpha_, button->imgWidth_, button->imgHeight_ );
   else if( 0 != button->fontData_ )
      drawButton( *button, false );
   
/*
   if( ( 0 != button->releaseSoundData_ ) && ( 0 != button->releaseSoundLength_ ) )
   {
      audioQueue_t &q = getAudioQueue();
//      unsigned numCancelled ;
//      q.clear( numCancelled );
      if( !button->releaseSoundDecoded_ )
         q.queuePlayback( button->jsObj_, button->releaseSoundData_, button->releaseSoundLength_ );
      else
         q.queuePlayback( button->jsObj_, *(audioQueue_t::waveHeader_t *)button->releaseSoundData_ );

   }
*/
   if( ( 0 != button->releaseSoundData_ ) && ( 0 != button->releaseSoundLength_ ) )
   {
      audioOutPoll_t *ao ;
      if( button->releaseSoundDecoded_ 
          && 
          ( 0 != ( ao = audioOutPoll_t::get() ) ) )
      {
         ao->queuePlayback( *(audioQueue_t::waveHeader_t *)button->releaseSoundData_ );
      }
      else
      {
         audioQueue_t &q = getAudioQueue();
         unsigned int numCancelled = 0;
         q.clear(numCancelled);
         if( !button->releaseSoundDecoded_ )
            q.queuePlayback( button->jsObj_, button->releaseSoundData_, button->releaseSoundLength_ );
         else
            q.queuePlayback( button->jsObj_, *(audioQueue_t::waveHeader_t *)button->releaseSoundData_ );
      }
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
      JSObject *thisObj = JS_NewObject( cx, &jsButtonClass_, NULL, NULL );

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

            if( JS_GetProperty( cx, rhObj, "x", &vX ) && JSVAL_IS_INT( vX )
                &&
                JS_GetProperty( cx, rhObj, "y", &vY ) && JSVAL_IS_INT( vY )
                &&
                JS_GetProperty( cx, rhObj, "width", &vW ) && JSVAL_IS_INT( vW )
                &&
                JS_GetProperty( cx, rhObj, "height", &vH ) && JSVAL_IS_INT( vH ) )
            {
               JS_DefineProperty( cx, thisObj, "x", vX, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "y", vY, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "width", vW, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
               JS_DefineProperty( cx, thisObj, "height", vH, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );

               buttonData_t *const buttonData = new buttonData_t ;
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
               buttonData->cx_      = cx ;
               buttonData->jsObj_   = thisObj ;
               box->onTouch_        = buttonTouch ;
               box->onTouchMove_    = buttonMove ;
               box->onTouchMoveOff_ = buttonMoveOff ;
               box->onRelease_      = buttonRelease ;

               jsval     jsv ;
               JSObject *jsO ;
               if( JS_GetProperty( cx, rhObj, "touchSound", &jsv ) && JSVAL_IS_OBJECT( jsv ) && ( 0 != ( jsO = JSVAL_TO_OBJECT( jsv ) ) ) )
               {
                  JS_DefineProperty( cx, thisObj, "touchSound", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  if( JS_GetProperty( cx, jsO, "data", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  {
                     JSString *sVal = JSVAL_TO_STRING(jsv);
                     buttonData->touchSoundData_ = (unsigned char *)JS_GetStringBytes( sVal );
                     buttonData->touchSoundLength_ = JS_GetStringLength( sVal );
                     buttonData->touchSoundDecoded_ = 0 ;
                  }
                  else if( JS_GetProperty( cx, jsO, "decoded", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  {
                     JSString *sVal = JSVAL_TO_STRING(jsv);
                     buttonData->touchSoundData_ = (unsigned char *)JS_GetStringBytes( sVal );
                     buttonData->touchSoundLength_ = JS_GetStringLength( sVal );
                     buttonData->touchSoundDecoded_ = 1 ;
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
                     buttonData->releaseSoundDecoded_ = 0 ;
                  }
                  else if( JS_GetProperty( cx, jsO, "decoded", &jsv ) && JSVAL_IS_STRING( jsv ) )
                  {
                     JSString *sVal = JSVAL_TO_STRING(jsv);
                     buttonData->releaseSoundData_ = (unsigned char *)JS_GetStringBytes( sVal );
                     buttonData->releaseSoundLength_ = JS_GetStringLength( sVal );
                     buttonData->releaseSoundDecoded_ = 1 ;
                  }
               }

               if( JS_GetProperty( cx, rhObj, "onTouch", &jsv ) )
               {
                  JSType type = JS_TypeOfValue( cx, jsv );
                  if( ( JSTYPE_STRING == type ) ||  ( JSTYPE_FUNCTION == type ) )
                     JS_DefineProperty( cx, thisObj, "onTouch", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  else if( JSTYPE_VOID != type )
                     JS_ReportError( cx, "Invalid onTouch handler\n" );
               }
               if( JS_GetProperty( cx, rhObj, "onMove", &jsv ) )
               {
                  JSType type = JS_TypeOfValue( cx, jsv );
                  if( ( JSTYPE_STRING == type ) ||  ( JSTYPE_FUNCTION == type ) )
                     JS_DefineProperty( cx, thisObj, "onMove", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  else if( JSTYPE_VOID != type )
                     JS_ReportError( cx, "Invalid onMove handler" );
               }
               if( JS_GetProperty( cx, rhObj, "onMoveOff", &jsv ) )
               {
                  JSType type = JS_TypeOfValue( cx, jsv );
                  if( ( JSTYPE_STRING == type ) ||  ( JSTYPE_FUNCTION == type ) )
                     JS_DefineProperty( cx, thisObj, "onMoveOff", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  else if( JSTYPE_VOID != type )
                     JS_ReportError( cx, "Invalid onMoveOff handler" );
               }
               if( JS_GetProperty( cx, rhObj, "onRelease", &jsv ) )
               {
                  JSType type = JS_TypeOfValue( cx, jsv );
                  if( ( JSTYPE_STRING == type ) ||  ( JSTYPE_FUNCTION == type ) )
                     JS_DefineProperty( cx, thisObj, "onRelease", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  else if( JSTYPE_VOID != type )
                     JS_ReportError( cx, "Invalid onRelease handler" );
               }

               JS_SetPrivate( cx, thisObj, buttonData );

               jsval     vImage ;
               JSObject *oImage ;
               if( JS_GetProperty( cx, rhObj, "img", &vImage ) && JSVAL_IS_OBJECT( vImage ) 
                   &&
                   ( 0 != ( oImage = JSVAL_TO_OBJECT( vImage ) ) ) )
               {
                  JS_DefineProperty( cx, thisObj, "image", vImage, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
   
                  jsval drawParams[2] = {
                     vX, vY
                  };
   
                  jsval drawResult ;
                  jsImageDraw( cx, oImage, 2, drawParams, &drawResult );

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
               } // get image button fields
               else if( JS_GetProperty( cx, rhObj, "font", &jsv ) 
                        && 
                        JSVAL_IS_OBJECT( jsv ) )
               {
                  JS_DefineProperty( cx, thisObj, "font", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  JSObject *const fontObj = JSVAL_TO_OBJECT( jsv );
                  if( JS_GetProperty( cx, fontObj, "data", &jsv ) 
                      &&
                      JSVAL_IS_STRING( jsv ) )
                  {
                     JSString *const sFont = JSVAL_TO_STRING( jsv );
                     buttonData->fontData_ = JS_GetStringBytes( sFont );
                     buttonData->fontSize_ = JS_GetStringLength( sFont );
   
                     buttonData->bgColor_      = 0x808080 ;
                     buttonData->textColor_    = 0 ;
                     buttonData->buttonWidth_  = w ;
                     buttonData->buttonHeight_ = h ;
                     buttonData->pointSize_    = 10 ;
                     buttonData->borderWidth_  = 2 ;
   
                     if( JS_GetProperty( cx, rhObj, "text", &jsv ) )
                     {
                        JSString * const msg = JSVAL_TO_STRING( jsv );
                        unsigned const len = JS_GetStringLength( msg );
                        char *newMsg = new char[ len + 1 ];
                        memcpy( newMsg, JS_GetStringBytes( msg ), len );
                        newMsg[len] = '\0' ;
                        buttonData->msgString_ = newMsg ;
                        JS_DefineProperty( cx, thisObj, "text", jsv, 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                     } // have button text
                     else
                     {
                        JS_ReportError( cx, "missing button text" );
                        buttonData->msgString_ = "missing" ;
                     }
                     
                     if( JS_GetProperty( cx, rhObj, "pointSize", &jsv ) 
                         &&
                         JSVAL_IS_INT( jsv ) )
                     {
                        buttonData->pointSize_ = JSVAL_TO_INT( jsv );
                     }
                     
                     if( JS_GetProperty( cx, rhObj, "bgColor", &jsv ) 
                         &&
                         JSVAL_IS_INT( jsv ) )
                     {
                        buttonData->bgColor_ = JSVAL_TO_INT( jsv );
                     }
   
                     if( JS_GetProperty( cx, rhObj, "textColor", &jsv ) 
                         &&
                         JSVAL_IS_INT( jsv ) )
                     {
                        buttonData->textColor_ = JSVAL_TO_INT( jsv );
                     }
   
                     if( JS_GetProperty( cx, rhObj, "borderWidth", &jsv ) 
                         &&
                         JSVAL_IS_INT( jsv ) )
                     {
                        buttonData->borderWidth_ = JSVAL_TO_INT( jsv );
                     }
                     
                     drawButton( *buttonData, false );
                  
                  }
                  else
                     JS_ReportError( cx, "missing font data" );
               } // get font fields
               
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

static JSBool
jsButtonChangeText( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   buttonData_t *const button = (buttonData_t *)JS_GetPrivate( cx, obj );
   if( button )
   {
      JSString *sText ;
      if( ( 1 == argc )
          &&
          JSVAL_IS_STRING( argv[0] )
          &&
          ( 0 != ( sText = JSVAL_TO_STRING( argv[0] ) ) ) )
      {
         JS_DefineProperty( cx, obj, "text", argv[0], 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
         unsigned const len = JS_GetStringLength( sText );
         char *newMsg = new char[ len + 1 ];
         memcpy( newMsg, JS_GetStringBytes( sText ), len );
         newMsg[len] = '\0' ;
         if( button->msgString_ )
            delete (char *)button->msgString_ ;
         button->msgString_= newMsg ;
         drawButton( *button, false );
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "usage: button.changeText( 'string' );" );
   }
   else
      JS_ReportError( cx, "Invalid button data" );
   
   return JS_TRUE ;

}

static JSBool
jsButtonDraw( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   buttonData_t *const button = (buttonData_t *)JS_GetInstancePrivate( cx, obj, &jsButtonClass_, NULL );
   if( button )
   {
      box_t &box = *button->box_ ;
   
      if( 0 != button->img_ )
         display( box.xLeft_, box.yTop_, button->img_, button->imgAlpha_, button->imgWidth_, button->imgHeight_ );
   }
   else
      JS_ReportError( cx, "Invalid button data" );
   
   return JS_TRUE ;
}

static JSBool
jsButtonDeactivate( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   buttonData_t *const button = (buttonData_t *)JS_GetInstancePrivate( cx, obj, &jsButtonClass_, NULL );
   if( button )
   {
      if( button->box_ )
      {
         assert( button == (buttonData_t *)button->box_->objectData_ );
         getZMap().removeBox( *button->box_ );
         destroyBox( button->box_ );
         button->box_ = 0 ;
      }
   }
   else
      JS_ReportError( cx, "Invalid button data" );
   
   return JS_TRUE ;
}

static JSBool jsGetX( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE;
   buttonData_t *const button = (buttonData_t *)JS_GetInstancePrivate( cx, obj, &jsButtonClass_, NULL );
   if (button) {
      *rval = INT_TO_JSVAL(button->getX);
   } else
      JS_ReportError( cx, "Invalid button data" );
   return JS_TRUE ;
}

static JSBool jsGetY( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE;
   buttonData_t *const button = (buttonData_t *)JS_GetInstancePrivate( cx, obj, &jsButtonClass_, NULL );
   if (button) {
      *rval = INT_TO_JSVAL(button->getY);
   } else
      JS_ReportError( cx, "Invalid button data" );
   return JS_TRUE ;
}

static JSFunctionSpec buttonMethods_[] = {
    {"changeText", jsButtonChangeText, 0 },
    {"draw",       jsButtonDraw, 0 },
    {"deactivate", jsButtonDeactivate, 0 },
    {"getX",	jsGetX, 0 },
    {"getY",	jsGetY, 0 },
    {0}
};

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


