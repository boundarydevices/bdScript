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
 * Revision 1.6  2002-11-30 23:45:49  ericn
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

class jsTouchScreenThread_t : public touchScreenThread_t {
public:
   jsTouchScreenThread_t( JSContext *cx,
                          JSObject  *scope )
      : touchScreenThread_t(),
        lastX_( 0 ),
        lastY_( 0 ),
        cx_( cx ),
        scope_( scope ),
        curBox_( 0 ){}
   virtual ~jsTouchScreenThread_t( void ){}

   virtual void onTouch( unsigned        x, 
                         unsigned        y );
   virtual void onRelease( void );
   
   virtual void onMove( unsigned        x, 
                        unsigned        y );

   int               lastX_ ;
   int               lastY_ ;

   JSContext * const cx_ ;
   JSObject  * const scope_ ;
   box_t            *curBox_ ;
};

static jsval onTouchCode_ = JSVAL_VOID ;

static JSBool
jsOnTouch( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sCode = JSVAL_TO_STRING( argv[0] );
      if( sCode )
      {
         onTouchCode_ = argv[0] ;
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "Error reading code param" );
   }

   return JS_TRUE ;
}

static jsval onReleaseCode_ = JSVAL_VOID ;

static JSBool
jsOnRelease( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sCode = JSVAL_TO_STRING( argv[0] );
      if( sCode )
      {
         onReleaseCode_ = argv[0] ;
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "Error reading code param" );
   }

   return JS_TRUE ;
}

static jsval onMoveCode_ = JSVAL_VOID ;

static JSBool
jsOnMove( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sCode = JSVAL_TO_STRING( argv[0] );
      if( sCode )
      {
         onMoveCode_ = argv[0] ;
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "Error reading code param" );
   }

   return JS_TRUE ;
}

static jsTouchScreenThread_t *thread_ = 0 ;

void jsTouchScreenThread_t :: onTouch
   ( unsigned x, 
     unsigned y )
{
   lastX_ = x ;
   lastY_ = y ;

   if( 0 != curBox_ )
   {
      if( ( x >= curBox_->xLeft_ )
          &&
          ( x < curBox_->xRight_ )
          &&
          ( y >= curBox_->yTop_ )
          &&
          ( y < curBox_->yBottom_ ) )
      {
         curBox_->onTouch_( *curBox_, x, y );
         return ;
      } // still on this button
      else
      {
         curBox_->onRelease_( *curBox_, x, y );
         curBox_ = 0 ;
      } // moved off of the button
   } // already touching, move or release

   std::vector<box_t *> boxes = getZMap().getBoxes( x, y );
   if( 0 < boxes.size() )
   {
      curBox_ = boxes[0];
      curBox_->onTouch_( *boxes[0], x, y );
   }
   else
   {
      if( JSVAL_VOID != onTouchCode_ )
         queueSource( scope_, onTouchCode_, "onTouch" );
      else
      {
         printf( "no touch handler %u/%u\n", x, y );
//         dumpZMaps();
      }
   } // no boxes... look for global handler
}


void jsTouchScreenThread_t :: onRelease( void )
{
   if( 0 != curBox_ )
   {
      curBox_->onRelease_( *curBox_, lastX_, lastY_ );
      curBox_ = 0 ;
   } // touching, move or release box

   if( JSVAL_VOID != onReleaseCode_ )
      queueSource( scope_, onReleaseCode_, "onRelease" );
}


void jsTouchScreenThread_t :: onMove
   ( unsigned x, 
     unsigned y )
{
   lastX_ = x ;
   lastY_ = y ;

   printf( "move %u/%u\n", x, y );
   if( 0 == curBox_ )
      onTouch( x, y );

   if( JSVAL_VOID != onMoveCode_ )
      queueSource( scope_, onMoveCode_, "onMove" );
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
   if( 0 != thread_ )
      *rval = INT_TO_JSVAL( thread_->lastX_ );
   else
      *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static JSBool
jsGetTouchY( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( 0 != thread_ )
      *rval = INT_TO_JSVAL( thread_->lastY_ );
   else
      *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static JSFunctionSpec touchMethods_[] = {
    {"getX",         jsGetTouchX,           0 },
    {"getY",         jsGetTouchY,           0 },
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
      JSObject *thisObj = js_NewObject( cx, &jsTouchClass_, NULL, NULL );

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


bool initJSTouch( touchScreenThread_t *&thread,
                  JSContext            *cx, 
                  JSObject             *glob )
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
         thread_ = new jsTouchScreenThread_t( cx, glob );
         if( thread_->begin() )
         {
            thread = thread_ ;
            JS_AddRoot( cx, &onTouchCode_ );
            JS_AddRoot( cx, &onMoveCode_ );
            JS_AddRoot( cx, &onReleaseCode_ );

            return true ;
         }
         else
         {
            delete thread_ ;
            thread_ = 0 ;
            JS_ReportError( cx, "Error starting touch screen thread\n" );
         }
      }
   }
   thread = 0 ;
   return false ;
}


