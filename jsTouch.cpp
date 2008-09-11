/*
 * Module jsTouch.cpp
 *
 * This module defines the Javascript support 
 * routines for touch-screens.
 *
 *
 * Change History : 
 *
 * $Log: jsTouch.cpp,v $
 * Revision 1.30  2008-09-11 00:27:55  ericn
 * [jsTouch] Remove use of wire count for calibration settings
 *
 * Revision 1.29  2008-06-25 01:19:38  ericn
 * add real mouse support (Davinci only)
 *
 * Revision 1.28  2007-07-03 18:10:37  ericn
 * -Optional UCB1400 GPIO
 *
 * Revision 1.27  2006/09/30 18:31:01  ericn
 * -add getPin(), setPin() methods
 *
 * Revision 1.26  2006/09/25 18:48:16  ericn
 * -add dump() method
 *
 * Revision 1.25  2006/05/14 14:35:38  ericn
 * -add touchTime(), releaseTime() methods to measure latency
 *
 * Revision 1.24  2005/11/06 00:49:43  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.23  2004/12/28 03:35:12  ericn
 * -added shutdown routine
 *
 * Revision 1.22  2004/11/27 18:00:34  ericn
 * -only 6 params
 *
 * Revision 1.21  2004/11/26 15:33:51  ericn
 * -remove range from touch calibration
 *
 * Revision 1.20  2004/11/16 15:48:59  ericn
 * -matrix based calibration
 *
 * Revision 1.19  2004/11/16 04:06:51  ericn
 * -jsNewTouch.cpp -> jsTouch.cpp
 *
 * Revision 1.5  2004/02/04 21:26:13  ericn
 * -updated to use flashVar for touch settings
 *
 * Revision 1.4  2004/01/02 23:36:07  ericn
 * -debugPrint()
 *
 * Revision 1.3  2003/12/03 12:53:08  ericn
 * -added swapXY support
 *
 * Revision 1.2  2003/11/25 00:07:49  ericn
 * -modified so touchScreen.getX() works properly in handlers
 *
 * Revision 1.1  2003/11/24 19:42:42  ericn
 * -polling touch screen
 *
 * Revision 1.17  2003/02/01 18:14:02  ericn
 * -added touchScreen.reset() method
 *
 * Revision 1.16  2003/01/31 13:28:20  ericn
 * -modified to stay on button at edges, added touchScreen global
 *
 * Revision 1.15  2003/01/08 15:20:49  ericn
 * -modified to prevent trailing touch
 *
 * Revision 1.14  2003/01/06 04:29:18  ericn
 * -made callbacks return bool (false if system shutting down)
 *
 * Revision 1.13  2003/01/05 01:56:57  ericn
 * -moved AddRoot calls
 *
 * Revision 1.12  2002/12/26 19:41:14  ericn
 * -removed debug statements
 *
 * Revision 1.11  2002/12/26 19:26:59  ericn
 * -added onMoveOff support
 *
 * Revision 1.10  2002/12/26 19:04:26  ericn
 * -lock touch flags, execute code directly from Javascript thread
 *
 * Revision 1.9  2002/12/26 18:18:28  ericn
 * -modified to throttle touch events
 *
 * Revision 1.8  2002/12/15 20:01:04  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.7  2002/12/14 23:59:58  ericn
 * -removed redundant touch code
 *
 * Revision 1.6  2002/11/30 23:45:49  ericn
 * -rooted touch handlers
 *
 * Revision 1.5  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.4  2002/11/30 17:32:47  ericn
 * -added support for touch-by-move
 *
 * Revision 1.3  2002/11/21 14:09:30  ericn
 * -preliminary button support
 *
 * Revision 1.2  2002/11/08 13:57:35  ericn
 * -removed debug msgs
 *
 * Revision 1.1  2002/11/03 15:39:36  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "jsTouch.h"
#include "js/jscntxt.h"
#include "codeQueue.h"
#include "zOrder.h"
#include "jsGlobals.h"
#include "touchPoll.h"
// #define DEBUGPRINT
#include "debugPrint.h"
#include "flashVar.h"
#include "ucb1x00_pins.h"
#include <math.h>
#include "touchCalibrate.h"

#include "config.h"
#ifdef CONFIG_MCP_UCB1400_TS
#include "linux/ucb1x00-adc.h"
#endif

class jsTouchPoll_t : public touchPoll_t {
public:
   jsTouchPoll_t( void );

   ~jsTouchPoll_t( void );

   virtual void onTouch( int x, int y, unsigned pressure, timeval const &tv );
   virtual void onMove( int x, int y, unsigned pressure, timeval const &tv );
   virtual void onRelease( timeval const &tv );
   void translate( int &x, int &y ) const ;

// protected:
   box_t    *curBox_ ;
   bool      wasDown_ ;
   int       prevX_ ;
   int       prevY_ ;
   jsval     onTouchObject_ ;
   jsval     onTouchCode_ ;
   jsval     onReleaseObject_ ;
   jsval     onReleaseCode_ ;
   jsval     onMoveObject_ ;
   jsval     onMoveCode_ ;
   bool      raw_ ;
   unsigned  numTouches_ ;
   int       lastX_[4];
   int       lasyY_[4];
   int       xSum_ ;
   int       ySum_ ;
};

jsTouchPoll_t :: jsTouchPoll_t( void )
   : touchPoll_t( pollHandlers_ )
   , curBox_( 0 )
   , wasDown_( true )
   , prevX_( -1 )
   , prevY_( -1 )
   , onTouchObject_( JSVAL_VOID )
   , onTouchCode_( JSVAL_VOID )
   , onReleaseObject_( JSVAL_VOID )
   , onReleaseCode_( JSVAL_VOID )
   , onMoveObject_( JSVAL_VOID )
   , onMoveCode_( JSVAL_VOID )
   , raw_( true )
   , numTouches_( 0 )
   , xSum_( 0 )
   , ySum_( 0 )
{
   char temp[80];
   fbDevice_t &fb = getFB();
   snprintf( temp, sizeof(temp), "t%ux%u", fb.getWidth(), fb.getHeight() );
   char const *flashVar = readFlashVar( temp );
   debugPrint( "calibrate var:<%s> == %s\n", temp, flashVar );
   if( flashVar )
   {
      raw_ = false ;
   }
   else
      fprintf( stderr, "No touch screen settings, using raw input\n" );
}

void jsTouchPoll_t :: translate( int &x, int &y ) const 
{
   if( !raw_ )
   {
      point_t in ; in.x = x ; in.y = y ;
      point_t out = in ;

      touchCalibration_t::get().translate(in,out);
      x = out.x ;
      y = out.y ;
   }
}

jsTouchPoll_t :: ~jsTouchPoll_t( void )
{
}

static timeval touchTime_ = { 0 };
static timeval releaseTime_ = { 0 };

void jsTouchPoll_t :: onTouch( int x, int y, unsigned pressure, timeval const &tv )
{
   debugPrint( "raw: %d/%d\n", x, y );
   translate( x, y );
   debugPrint( "cooked: %d/%d\n", x, y );

   prevX_ = x ; // set here so code has access to it
   prevY_ = y ;

   if( 0 != curBox_ )
   {
      onMove( x, y, pressure, tv );
   } // already touching, move or release
   else
   {
      touchTime_ = tv ;
      std::vector<box_t *> boxes = getZMap().getBoxes( x, y );
      if( 0 < boxes.size() )
      {
         curBox_ = boxes[0];
         curBox_->onTouch_( *boxes[0], x, y );
      }
      else
      {
         if( JSVAL_VOID != onTouchCode_ )
         {
            executeCode( JSVAL_TO_OBJECT( onTouchObject_ ), onTouchCode_, "onTouch" );
         }
         else
         {
//            printf( "no touch handler %u/%u\n", x, y );
//            dumpZMaps();
         }
      } // no boxes... look for global handler
   }

   wasDown_ = true ;
}

void jsTouchPoll_t :: onMove( int x, int y, unsigned pressure, timeval const &tv )
{
   if( 0 != curBox_ )
   {
      if( ( x >= curBox_->xLeft_ )
          &&
          ( x <= curBox_->xRight_ )
          &&
          ( y >= curBox_->yTop_ )
          &&
          ( y <= curBox_->yBottom_ ) )
      {
         curBox_->onTouchMove_( *curBox_, x, y );
         return ;
      } // still on this button
      else
      {
         curBox_->onTouchMoveOff_( *curBox_, x, y );
         curBox_ = 0 ;
      } // moved off of the button
   } // have a box
   
   std::vector<box_t *> boxes = getZMap().getBoxes( x, y );
   if( 0 < boxes.size() )
   {
      curBox_ = boxes[0];
      curBox_->onTouch_( *boxes[0], x, y );
   }
   else
   {
      if( JSVAL_VOID != onMoveCode_ )
         executeCode( JSVAL_TO_OBJECT( onMoveObject_ ), onMoveCode_, "onMove" );
      else
      {
//         printf( "no touch handler %u/%u\n", x, y );
//         dumpZMaps();
      }
   } // no boxes... look for global handler
}

void jsTouchPoll_t :: onRelease( timeval const &tv )
{
   if( 0 != curBox_ )
   {
      releaseTime_ = tv ;
      curBox_->onRelease_( *curBox_, prevX_, prevY_ );
      curBox_ = 0 ;
   } // touching, move or release box
   else if( JSVAL_VOID != onReleaseCode_ )
      executeCode( JSVAL_TO_OBJECT( onReleaseObject_ ), onReleaseCode_, "onRelease" );
   wasDown_ = false ;
}

static jsTouchPoll_t *touchPoll_ = 0 ;

void onRelease( void )
{
   if( touchPoll_ ){
      timeval now ;
      gettimeofday(&now, 0);
      touchPoll_->onRelease(now);
   }
}

static JSBool
doit( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval, 
      jsval &handler,
      jsval &handlerObj )
{
   *rval = JSVAL_FALSE ;
   
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sCode = JSVAL_TO_STRING( argv[0] );
      if( sCode )
      {
         handler = argv[0] ;
         handlerObj = OBJECT_TO_JSVAL( obj );
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "touchScreen:error reading code param" );
   }
   else if( 0 == argc )
   {
      handler = handlerObj = JSVAL_VOID ;
   }

   return JS_TRUE ;
}

static JSBool
jsOnTouch( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return doit( cx, obj, argc, argv, rval, touchPoll_->onTouchCode_, touchPoll_->onTouchObject_ );
}

static JSBool
jsOnRelease( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return doit( cx, obj, argc, argv, rval, touchPoll_->onReleaseCode_, touchPoll_->onReleaseObject_ );
}

static JSBool
jsOnMove( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return doit( cx, obj, argc, argv, rval, touchPoll_->onMoveCode_, touchPoll_->onMoveObject_ );
}


static JSFunctionSpec touch_functions[] = {
    {"onTouch",         jsOnTouch,        1 },
    {"onRelease",       jsOnRelease,      1 },
    {"onMove",          jsOnMove,         1 },
    {0}
};

static JSBool
jsGetTouchX( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = INT_TO_JSVAL( touchPoll_->prevX_ );
   return JS_TRUE ;
}

static JSBool
jsGetTouchY( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = INT_TO_JSVAL( touchPoll_->prevY_ );
   return JS_TRUE ;
}

static JSBool
jsIsRaw( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = touchPoll_->raw_ ? JSVAL_TRUE : JSVAL_FALSE ;
   
   return JS_TRUE ;
}

static JSBool
jsSetRaw( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   touchPoll_->raw_ = true ;
   *rval = JSVAL_VOID ;
   return JS_TRUE ;
}

static JSBool
jsTouchTime( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = INT_TO_JSVAL( (touchTime_.tv_sec*1000)+(touchTime_.tv_usec / 1000) );

   return JS_TRUE ;
}

static JSBool
jsReleaseTime( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = INT_TO_JSVAL( (releaseTime_.tv_sec*1000)+(releaseTime_.tv_usec / 1000) );

   return JS_TRUE ;
}

static JSBool
jsDump( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( touchPoll_ ){
      touchPoll_->dump();
      *rval = JSVAL_TRUE ;
   }
   else {
      *rval = JSVAL_FALSE ;
      JS_ReportError( cx, "No touch screen object\n" );
   }

   return JS_TRUE ;
}

#ifdef CONFIG_MCP_UCB1400_TS
static JSBool
jsGetPin( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 1 == argc )
       &&
       JSVAL_IS_INT( argv[0] ) )
   {
      unsigned pinNum = JSVAL_TO_INT(argv[0]);
      if( ( pinNum >= NUMADCPINS ) &&
          ( pinNum < NUMGPIOPINS ) ){
         bool value ;
         if( ucb1x00_get_pin( touchPoll_->getFd(), pinNum, value ) ){
            if( value )
               *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "Error reading pin %u\n", pinNum );
      }
      else
         JS_ReportError( cx, "Invalid pin (range [%u..%u])\n", NUMADCPINS, NUMGPIOPINS-1 );
   }
   else
      JS_ReportError( cx, "Usage: touchScreen.getPin( pinNum[%u..%u])\n", NUMADCPINS, NUMGPIOPINS-1 );

   return JS_TRUE ;
}

static JSBool
jsSetPin( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 2 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] ) )
   {
      unsigned pinNum = JSVAL_TO_INT(argv[0]);
      if( ( pinNum >= NUMADCPINS ) &&
          ( pinNum < NUMGPIOPINS ) ){
         unsigned value = JSVAL_TO_INT(argv[1]);
         if( (0 == value) || (1 == value) ){
            if( ucb1x00_set_pin( touchPoll_->getFd(), pinNum, (1==value) ) ){
               *rval = JSVAL_TRUE ;
            }
            else
               JS_ReportError( cx, "Error reading pin %u\n", pinNum );
         }
         else
            JS_ReportError( cx, "Invalid value: should be zero or one\n" );
      }
      else
         JS_ReportError( cx, "Invalid pin (range [%u..%u])\n", NUMADCPINS, NUMGPIOPINS-1 );
   }
   else
      JS_ReportError( cx, "Usage: touchScreen.setPin( pinNum[%u..%u], 0|1)\n", NUMADCPINS, NUMGPIOPINS-1 );


   return JS_TRUE ;
}

#endif

static JSFunctionSpec touchMethods_[] = {
    {"getX",         jsGetTouchX,           0 },
    {"getY",         jsGetTouchY,           0 },
    {"isRaw",        jsIsRaw,               0 },
    {"setRaw",       jsSetRaw,              0 },
    {"touchTime",    jsTouchTime,           0 },
    {"releaseTime",  jsReleaseTime,         0 },
#ifdef CONFIG_MCP_UCB1400_TS
    {"getPin",       jsGetPin,              0 },
    {"setPin",       jsSetPin,              0 },
#endif
    {"dump",         jsDump,                0 },
    {0}
};

JSClass jsTouchClass_ = {
  "touchScreen",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec touchProperties_[] = {
  {0,0,0}
};


static JSBool touch( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 0 == argc ) 
       && 
       ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsTouchClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
      }
      else
         JS_ReportError( cx, "Error allocating touchScreen" );
   }
   else
      JS_ReportError( cx, "Usage : new touchScreen();" );
      
   return JS_TRUE ;

}

bool initJSTouch( JSContext *cx, 
                  JSObject  *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsTouchClass_,
                                  touch, 1,
                                  touchProperties_, 
                                  touchMethods_,
                                  0, 0 );
   if( rval )
   {
      if( JS_DefineFunctions( cx, glob, touch_functions ) )
      {
         JSObject *obj = JS_NewObject( cx, &jsTouchClass_, NULL, NULL );
         if( obj )
         {
            //
            // root
            //
            JS_DefineProperty( cx, glob, "touchScreen", 
                               OBJECT_TO_JSVAL( obj ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            touchPoll_ = new jsTouchPoll_t ;
            JS_AddRoot( cx, &touchPoll_->onTouchCode_ );
            JS_AddRoot( cx, &touchPoll_->onMoveCode_ );
            JS_AddRoot( cx, &touchPoll_->onReleaseCode_ );
            JS_AddRoot( cx, &touchPoll_->onTouchObject_ );
            JS_AddRoot( cx, &touchPoll_->onReleaseObject_ );
            JS_AddRoot( cx, &touchPoll_->onMoveObject_ );
            return true ;
         }
         else
            JS_ReportError( cx, "defining touch screen" );

      }
   }
   return false ;
}

void shutdownTouch( void )
{
   if( touchPoll_ )
   {
      delete touchPoll_ ;
      touchPoll_ = 0 ;
   }
}
