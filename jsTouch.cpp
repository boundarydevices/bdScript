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
 * Revision 1.16  2003-01-31 13:28:20  ericn
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
#include "semClasses.h"

static mutex_t mutex_ ;

class jsTouchScreenThread_t : public touchScreenThread_t {
public:
   jsTouchScreenThread_t( JSContext *cx,
                          JSObject  *scope )
      : touchScreenThread_t(),
        lastX_( 0 ),
        lastY_( 0 ),
        flags_( 0 ),
        cx_( cx ),
        scope_( scope ),
        curBox_( 0 ){}
   virtual ~jsTouchScreenThread_t( void ){ printf( "closing ts thread\n" ); close(); }

   // these are called directly by the touch screen thread
   virtual bool onTouch( unsigned        x, 
                         unsigned        y );
   virtual bool onRelease( void );
   
   virtual bool onMove( unsigned        x, 
                        unsigned        y );

   enum {
      queuedTouch_   = 1,
      queuedRelease_ = 2
   };

   int               lastX_ ;
   int               lastY_ ;
   int               flags_ ; // enums above used to throttle delivery

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

static void doOnMove( void *data )
{
   assert( data == (void *)thread_ );
   int const x = thread_->lastX_ ;
   int const y = thread_->lastY_ ;
   
   if( 0 != thread_->curBox_ )
   {
      if( ( x >= thread_->curBox_->xLeft_ )
          &&
          ( x <= thread_->curBox_->xRight_ )
          &&
          ( y >= thread_->curBox_->yTop_ )
          &&
          ( y <= thread_->curBox_->yBottom_ ) )
      {
         thread_->curBox_->onTouchMove_( *thread_->curBox_, x, y );
         mutexLock_t lock( mutex_ );
         thread_->flags_ &= ~thread_->queuedTouch_ ;
        return ;
      } // still on this button
      else
      {
         thread_->curBox_->onTouchMoveOff_( *thread_->curBox_, x, y );
         thread_->curBox_ = 0 ;
      } // moved off of the button
   } // have a box
   
   std::vector<box_t *> boxes = getZMap().getBoxes( x, y );
   if( 0 < boxes.size() )
   {
      thread_->curBox_ = boxes[0];
      thread_->curBox_->onTouch_( *boxes[0], x, y );
   }
   else
   {
      if( JSVAL_VOID != onMoveCode_ )
         executeCode( thread_->scope_, onMoveCode_, "onMove" );
      else
      {
//         printf( "no touch handler %u/%u\n", x, y );
//         dumpZMaps();
      }
   } // no boxes... look for global handler

   mutexLock_t lock( mutex_ );
   thread_->flags_ &= ~thread_->queuedTouch_ ;
}

//
// These routines are called in the context of the Javascript
// interpreter thread
//
static void doOnTouch( void *data )
{
   assert( data == (void *)thread_ );
   int const x = thread_->lastX_ ;
   int const y = thread_->lastY_ ;

   if( 0 != thread_->curBox_ )
   {
      doOnMove( data );
   } // already touching, move or release
   else
   {
      std::vector<box_t *> boxes = getZMap().getBoxes( x, y );
      if( 0 < boxes.size() )
      {
         thread_->curBox_ = boxes[0];
         thread_->curBox_->onTouch_( *boxes[0], x, y );
      }
      else
      {
         if( JSVAL_VOID != onTouchCode_ )
            executeCode( thread_->scope_, onTouchCode_, "onTouch" );
         else
         {
//            printf( "no touch handler %u/%u\n", x, y );
//            dumpZMaps();
         }
      } // no boxes... look for global handler
   
      mutexLock_t lock( mutex_ );
      thread_->flags_ &= ~thread_->queuedTouch_ ;
   }
}

static void doOnRelease( void *data )
{
   assert( data == (void *)thread_ );
   if( 0 != thread_->curBox_ )
   {
      thread_->curBox_->onRelease_( *thread_->curBox_, thread_->lastX_, thread_->lastY_ );
      thread_->curBox_ = 0 ;
   } // touching, move or release box
   else if( JSVAL_VOID != onReleaseCode_ )
      executeCode( thread_->scope_, onReleaseCode_, "onRelease" );
   
   mutexLock_t lock( mutex_ );
   thread_->flags_ &= ~thread_->queuedRelease_ ;
}

bool jsTouchScreenThread_t :: onTouch
   ( unsigned x, 
     unsigned y )
{
   mutexLock_t lock( mutex_ );
   if( 0 == ( flags_ & queuedTouch_ ) )
   {
      lastX_ = x ;
      lastY_ = y ;
   
      flags_ |= queuedTouch_ ;
      flags_ &= ~thread_->queuedRelease_ ; // make sure we get a subsequent release
      return queueCallback( doOnTouch, this );
   }
   else
      return true ;
}


bool jsTouchScreenThread_t :: onRelease( void )
{
   mutexLock_t lock( mutex_ );
   if( 0 == ( flags_ & queuedRelease_ ) )
   {
      flags_ |= queuedRelease_ ;
      flags_ &= ~thread_->queuedTouch_ ; // make sure we get a subsequent touch
      return queueCallback( doOnRelease, this );
   }
   else
      return true ;
}


bool jsTouchScreenThread_t :: onMove
   ( unsigned x, 
     unsigned y )
{
   mutexLock_t lock( mutex_ );
   if( 0 == ( flags_ & queuedTouch_ ) )
   {
      lastX_ = x ;
      lastY_ = y ;
   
      flags_ |= queuedTouch_ ;
      return queueCallback( doOnMove, this );
   }
   else
      return true ;
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
         JSObject *obj = JS_NewObject( cx, &jsTouchClass_, NULL, NULL );
         if( obj )
         {
            JS_DefineProperty( cx, glob, "touchScreen", 
                               OBJECT_TO_JSVAL( obj ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
         }
         else
            JS_ReportError( cx, "defining touch screen" );

         JS_AddRoot( cx, &onTouchCode_ );
         JS_AddRoot( cx, &onMoveCode_ );
         JS_AddRoot( cx, &onReleaseCode_ );

         thread_ = new jsTouchScreenThread_t( cx, glob );
         if( thread_->begin() )
         {
            thread = thread_ ;

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


