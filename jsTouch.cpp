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
 * Revision 1.19  2004-11-16 04:06:51  ericn
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
#include "debugPrint.h"
#include "flashVar.h"

class jsTouchPoll_t : public touchPoll_t {
public:
   jsTouchPoll_t( void );

   ~jsTouchPoll_t( void ){}

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
   bool      swapXY_ ;
   long      scaleX_ ;
   long      scaleY_ ;
   long      originX_ ;
   long      originY_ ;
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
   , swapXY_( false )
   , scaleX_( 256 )
   , scaleY_( 256 )
   , originX_( 0 )
   , originY_( 0 )
   , numTouches_( 0 )
   , xSum_( 0 )
   , ySum_( 0 )
{
   static char const calibrateVar[] = {
      "tsCalibrate"
   };

   char const *flashVar = readFlashVar( calibrateVar );
   if( flashVar )
   {
      debugPrint( "<%s>\n", flashVar );
      long scaleX, scaleY, originX, originY, rangeX, rangeY ;
      unsigned swap ;
      if( 7 == sscanf( flashVar, "%ld,%ld,%ld,%ld,%ld,%ld,%u", &scaleX, &scaleY, &originX, &originY, &rangeX, &rangeY, &swap ) )
      {
         swapXY_  = ( 0 != swap );
         scaleX_  = scaleX ;
         scaleY_  = scaleY ;
         originX_ = originX ;
         originY_ = originY ;
         touchPoll_t::setRange( rangeX, rangeY );
      }
      else
         fprintf( stderr, "Invalid calibration settings\n" );
   }
   else
      fprintf( stderr, "No touch screen settings, using raw input\n" );
}

void jsTouchPoll_t :: translate( int &x, int &y ) const 
{
   if( swapXY_ )
   {
      int tmp = x ;
      x = y ;
      y = tmp ;
   }
   x = ((x-originX_)*256)/scaleX_ ;
   y = ((y-originY_)*256)/scaleY_ ;
}

void jsTouchPoll_t :: onTouch( int x, int y, unsigned pressure, timeval const &tv )
{
   translate( x, y );
   prevX_ = x ; // set here so code has access to it
   prevY_ = y ;

   if( 0 != curBox_ )
   {
      onMove( x, y, pressure, tv );
   } // already touching, move or release
   else
   {
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
      curBox_->onRelease_( *curBox_, prevX_, prevY_ );
      curBox_ = 0 ;
   } // touching, move or release box
   else if( JSVAL_VOID != onReleaseCode_ )
      executeCode( JSVAL_TO_OBJECT( onReleaseObject_ ), onReleaseCode_, "onRelease" );
   wasDown_ = false ;
}

static jsTouchPoll_t *touchPoll_ = 0 ;

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
   if( ( 256 != touchPoll_->scaleY_ ) 
       ||
       ( 256 != touchPoll_->scaleY_ ) 
       ||
       ( 0 != touchPoll_->originX_ )
       ||
       ( 0 != touchPoll_->originY_ ) )
      *rval = JSVAL_FALSE ;
   else
      *rval = JSVAL_TRUE ;
   
   return JS_TRUE ;
}

static JSBool
jsSetRaw( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   touchPoll_->swapXY_  = 0 ;
   touchPoll_->scaleX_  = 256 ;
   touchPoll_->scaleY_  = 256 ;
   touchPoll_->originX_ = 0 ;
   touchPoll_->originY_ = 0 ;
   *rval = JSVAL_VOID ;
   return JS_TRUE ;
}

static JSBool
jsSetCooked( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc )
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *const rhObj = JSVAL_TO_OBJECT( argv[0] );
      jsval rhval ;
      if( JS_GetProperty( cx, rhObj, "scale", &rhval ) && JSVAL_IS_OBJECT( rhval ) )
      {
         JSObject *const scaleObj = JSVAL_TO_OBJECT( rhval );
         if( JS_GetProperty( cx, rhObj, "origin", &rhval ) && JSVAL_IS_OBJECT( rhval ) )
         {
            JSObject *const originObj = JSVAL_TO_OBJECT( rhval );
            if( JS_GetProperty( cx, rhObj, "range", &rhval ) && JSVAL_IS_OBJECT( rhval ) )
            {
               JSObject *const rangeObj = JSVAL_TO_OBJECT( rhval );
               jsval xval, yval ;
               if( JS_GetProperty( cx, scaleObj, "x", &xval ) && JSVAL_IS_INT( xval ) 
                   &&
                   JS_GetProperty( cx, scaleObj, "y", &yval ) && JSVAL_IS_INT( yval ) )
               {
                  int const scaleX = JSVAL_TO_INT( xval );
                  int const scaleY = JSVAL_TO_INT( yval );
                  if( JS_GetProperty( cx, originObj, "x", &xval ) && JSVAL_IS_INT( xval ) 
                      &&
                      JS_GetProperty( cx, originObj, "y", &yval ) && JSVAL_IS_INT( yval ) )
                  {
                     int const originX = JSVAL_TO_INT( xval );
                     int const originY = JSVAL_TO_INT( yval );
                     if( JS_GetProperty( cx, rangeObj, "x", &xval ) && JSVAL_IS_INT( xval ) 
                         &&
                         JS_GetProperty( cx, rangeObj, "y", &yval ) && JSVAL_IS_INT( yval ) )
                     {
                        int const rangeX = JSVAL_TO_INT( xval );
                        int const rangeY = JSVAL_TO_INT( yval );
                        if( JS_GetProperty( cx, rhObj, "swapXY", &rhval ) && JSVAL_IS_BOOLEAN( rhval ) )
                        {
                           bool const swapXY = JSVAL_TO_BOOLEAN( rhval );
                           debugPrint( "scale: %u/%u, %u/%u, %u/%u\n", scaleX, scaleY, originX, originY, rangeX, rangeY );
                           touchPoll_->scaleX_  = scaleX ;
                           touchPoll_->scaleY_  = scaleY ;
                           touchPoll_->originX_ = originX ;
                           touchPoll_->originY_ = originY ;
                           touchPoll_->setRange( rangeX, rangeY );
                           touchPoll_->swapXY_  = swapXY ;
   
                           *rval = JSVAL_TRUE ;
                           return JS_TRUE ;
                        }
                     }
                  }
               }
            }
         }
      }
   }

   JS_ReportError( cx, "Usage: touchScreen.setCooked( { scale:{ x:#, y:# }, origin:{ x:#, y:# }, range:{ x:#, y:# }, swapXY=bool } )" );

   *rval = JSVAL_VOID ;
   return JS_TRUE ;
}

static JSFunctionSpec touchMethods_[] = {
    {"getX",         jsGetTouchX,           0 },
    {"getY",         jsGetTouchY,           0 },
    {"isRaw",        jsIsRaw,               0 },
    {"setRaw",       jsSetRaw,              0 },
    {"setCooked",    jsSetCooked,           0 },
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


