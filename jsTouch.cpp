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
 * Revision 1.1  2002-11-03 15:39:36  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "jsTouch.h"
#include "js/jscntxt.h"
#include "codeQueue.h"

class jsTouchScreenThread_t : public touchScreenThread_t {
public:
   jsTouchScreenThread_t( JSContext *cx,
                          JSObject  *scope )
      : touchScreenThread_t(),
        lastX_( 0 ),
        lastY_( 0 ),
        cx_( cx ),
        scope_( scope ){}
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
};

static std::string onTouchCode_ ;

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
         char const *cCode = JS_GetStringBytes( sCode );
         if( cCode )
         {
            onTouchCode_ = cCode ;
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "Error reading code bytes" );
      }
      else
         JS_ReportError( cx, "Error reading code param" );
   }

   return JS_TRUE ;
}

static std::string onReleaseCode_ ;

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
         char const *cCode = JS_GetStringBytes( sCode );
         if( cCode )
         {
            onReleaseCode_ = cCode ;
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "Error reading code bytes" );
      }
      else
         JS_ReportError( cx, "Error reading code param" );
   }

   return JS_TRUE ;
}

static std::string onMoveCode_ ;

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
         char const *cCode = JS_GetStringBytes( sCode );
         if( cCode )
         {
            onMoveCode_ = cCode ;
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "Error reading code bytes" );
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
printf( "touch %u/%u\n", x, y );
   lastX_ = x ;
   lastY_ = y ;
   if( 0 != onTouchCode_.size() )
      queueSource( scope_, onTouchCode_, "onTouch" );
}


void jsTouchScreenThread_t :: onRelease( void )
{
printf( "release\n" );
   if( 0 != onReleaseCode_.size() )
      queueSource( scope_, onReleaseCode_, "onRelease" );
}


void jsTouchScreenThread_t :: onMove
   ( unsigned x, 
     unsigned y )
{
   lastX_ = x ;
   lastY_ = y ;

   if( 0 != onMoveCode_.size() )
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


