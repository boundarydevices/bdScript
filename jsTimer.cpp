/*
 * Module jsTimer.cpp
 *
 * This module defines the initialization routine
 * and Javascript support routines for timers, as
 * declared in jsTimer.h
 *
 *
 * Change History : 
 *
 * $Log: jsTimer.cpp,v $
 * Revision 1.10  2003-12-27 18:38:51  ericn
 * -got rid of secondary threads
 *
 * Revision 1.9  2003/06/22 23:03:42  ericn
 * -removed debug msg
 *
 * Revision 1.8  2003/06/16 12:54:59  ericn
 * -modified to clean up timer threads
 *
 * Revision 1.7  2003/01/05 01:58:15  ericn
 * -added identification of threads
 *
 * Revision 1.6  2002/12/15 00:00:44  ericn
 * -modified to allow 'interrupted system call status'
 *
 * Revision 1.5  2002/11/30 23:58:01  ericn
 * -rooted timer handlers
 *
 * Revision 1.4  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.3  2002/10/31 02:07:14  ericn
 * -modified to include scope in queueSource
 *
 * Revision 1.2  2002/10/27 17:48:57  ericn
 * -removed debug code
 *
 * Revision 1.1  2002/10/27 17:42:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsTimer.h"
#include <time.h>
#include <string>
#include "codeQueue.h"
#include "pollTimer.h"
#include <set>

struct timerObject_t : public pollTimer_t {
public:
   timerObject_t( JSContext    *cx, 
                  JSObject     *scope, 
                  unsigned long ms, 
                  jsval         source, 
                  bool          once );
   ~timerObject_t( void );

   virtual void fire( void );

   JSContext    *cx_ ;
   JSObject     *scope_ ;
   unsigned long milliseconds_ ;
   jsval         sourceCode_ ;
   bool const    once_ ;

private:
   timerObject_t( timerObject_t const & ); // no copies
};

typedef std::set<timerObject_t *> timerSet_t ;

static timerSet_t timers_ ;

timerObject_t :: timerObject_t( JSContext    *cx, 
                                JSObject     *scope, 
                                unsigned long ms, 
                                jsval         source, 
                                bool          once )
   : cx_( cx )
   , scope_( scope )
   , milliseconds_( ms )
   , sourceCode_( source )
   , once_( once )
{
   JS_AddRoot( cx_, &sourceCode_ );
   set( milliseconds_ );
}

timerObject_t :: ~timerObject_t( void )
{
   clear();
   timerSet_t :: iterator it = timers_.find( this );
   if( it != timers_.end() )
      timers_.erase( it );
   JS_RemoveRoot( cx_, &sourceCode_ );
}

void timerObject_t::fire( void )
{
//   printf( "timerObject_t::fire()\n" );
   executeCode( scope_, sourceCode_, "timer" );
   
   if( once_ )
      delete this ;
   else
      set( milliseconds_ );
}

static JSBool
jsTimer( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 <= argc ) 
       && 
       ( 3 >= argc ) 
       && 
       JSVAL_IS_INT( argv[0] ) )
   {
      JSType type = JS_TypeOfValue( cx, argv[1] );
      if( ( JSTYPE_STRING == type ) ||  ( JSTYPE_FUNCTION == type ) )
      {
         JSObject *scope = 0 ;
         if( 3 == argc )
         {
            if( JSVAL_IS_OBJECT( argv[2] ) )
            {
               scope = JSVAL_TO_OBJECT( argv[2] );
            }
            else
               JS_ReportError( cx, "timer: third parameter must be an object" );
         }
         else
            scope = obj ;

         if( 0 != scope )
         {
            timerObject_t *t = new timerObject_t( cx, scope, JSVAL_TO_INT( argv[0] ), argv[1], false );
            timers_.insert( t );
            *rval = INT_TO_JSVAL( (int)t );
         } // valid scope
      }
      else
         JS_ReportError( cx, "Usage: timer( unsigned long ms, 'handler code'||function [,scopeObject] )" );
   }
   else
      JS_ReportError( cx, "Usage: timer( unsigned long ms, 'handler code'||function [,scopeObject] )" );

   return JS_TRUE ;
}


static JSBool
jsOneShot( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 <= argc ) 
       && 
       ( 3 >= argc ) 
       && 
       JSVAL_IS_INT( argv[0] ) )
   {
      JSType type = JS_TypeOfValue( cx, argv[1] );
      if( ( JSTYPE_STRING == type ) ||  ( JSTYPE_FUNCTION == type ) )
      {
         JSObject *scope = 0 ;
         if( 3 == argc )
         {
            if( JSVAL_IS_OBJECT( argv[2] ) )
            {
               scope = JSVAL_TO_OBJECT( argv[2] );
            }
            else
               JS_ReportError( cx, "oneShot: third parameter must be an object" );

         }
         else
            scope = obj ;

         if( 0 != scope )
         {
            timerObject_t *t = new timerObject_t( cx, scope, JSVAL_TO_INT( argv[0] ), argv[1], true );
            timers_.insert( t );
            *rval = INT_TO_JSVAL( (int)t );
         } // valid scope
      }
      else
         JS_ReportError( cx, "Usage: oneShot( unsigned long ms, 'handler code'|function [,scopeObject] )" );
   }
   else
      JS_ReportError( cx, "Usage: oneShot( unsigned long ms, 'handler code'|function [,scopeObject] )" );

   return JS_TRUE ;
}


static JSBool
jsCancelTimer( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) 
       && 
       JSVAL_IS_INT( argv[0] ) )
   {
      timerObject_t *t = (timerObject_t *)JSVAL_TO_INT( argv[0] );
      timerSet_t :: iterator it = timers_.find( t );
      if( it != timers_.end() )
         delete t ;
      *rval = JSVAL_TRUE ;
   }
   else
      *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}


static JSFunctionSpec _functions[] = {
    {"timer",           jsTimer,          1 },
    {"oneShot",         jsOneShot,        1 },
    {"cancelTimer",     jsCancelTimer,    1 },
    {0}
};

bool initJSTimer( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}
