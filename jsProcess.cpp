/*
 * Module jsProcess.cpp
 *
 * This module defines the initialization routine and the
 * internals of the childProcess class as described in 
 * jsProcess.h
 *
 *
 * Change History : 
 *
 * $Log: jsProcess.cpp,v $
 * Revision 1.3  2003-08-24 22:01:51  ericn
 * -added shutdown routine (atexit doesn't appear to work) and use SIGABRT, not SIGKILL
 *
 * Revision 1.2  2003/08/24 18:33:04  ericn
 * -provide dummy args to child
 *
 * Revision 1.1  2003/08/24 17:32:20  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsProcess.h"
#include "childProcess.h"
#include <unistd.h>
#include "jsGlobals.h"
#include "codeQueue.h"

struct jsProcess_t : public childProcess_t {
   jsProcess_t( jsval        obj,
                char const  *exe,
                int          nargs,
                jsval const *args );
   virtual ~jsProcess_t( void );

   virtual void died( void );

   jsval                obj_ ;
   char const * const   exe_ ;
};

jsProcess_t :: jsProcess_t
   ( jsval       obj,
     char const *exe,
     int          nargs,
     jsval const *args )
   : obj_( obj ),
     exe_( strdup( exe ) )
{
   JS_AddRoot( execContext_, &obj_ );
   char **charArgs = new char *[ nargs+2 ];
   charArgs[0]       = (char *)exe_ ;
   for( int arg = 0 ; arg < nargs ; arg++ )
   {
      if( JSVAL_IS_STRING( args[arg] ) )
         charArgs[arg+1] = JS_GetStringBytes( JSVAL_TO_STRING( args[arg] ) );
      else
         charArgs[arg+1] = "notstring" ;
   }
   charArgs[nargs+1] = 0 ; // terminate

   run( exe_, charArgs, __environ );
   delete charArgs ;
}

jsProcess_t :: ~jsProcess_t( void )
{
   if( exe_ )
      free( (void *)exe_ );
   JS_RemoveRoot( execContext_, &obj_ );
}

void jsProcess_t :: died( void )
{
   JSObject *obj = JSVAL_TO_OBJECT( obj_ );
   if( obj )
   {
      jsval vHandler ;
      if( JS_GetProperty( execContext_, obj, "exitHandler", &vHandler ) )
      {
printf( "NOT queueing exit handler\n" ); fflush( stdout );
//         queueUnrootedSource( obj, vHandler, "childProcessExit" );
printf( "done queueing exit handler\n" ); fflush( stdout );
      }
   }
   
   printf( "process %s died\n", exe_ );
   pid_ = -1 ;
}

extern JSClass jsChildProcessClass_ ;

JSClass jsChildProcessClass_ = {
  "childProcess",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      0,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec childProcessProperties_[] = {
  {0,0,0}
};

static JSBool
jsChildProcessClose( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsProcess_t * const child = (jsProcess_t *)JS_GetInstancePrivate( cx, obj, &jsChildProcessClass_, NULL );
   if( 0 != child )
   {
      if( child->isRunning() )
      {
         int const result = kill( child->pid_, SIGABRT );
         if( 0 == result )
            *rval = JSVAL_TRUE ;
         else
            JS_ReportError( cx, "%m killing process %d(%s)\n", child->pid_, child->exe_ );
      }
      else
         JS_ReportError( cx, "process %d(%s) not running\n", child->pid_, child->exe_ );
   }

   return JS_TRUE ;
}

static JSBool
jsChildProcessIsRunning( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsProcess_t * const child = (jsProcess_t *)JS_GetInstancePrivate( cx, obj, &jsChildProcessClass_, NULL );
   if( 0 != child )
   {
      if( child->isRunning() )
         *rval = JSVAL_TRUE ;
   }

   return JS_TRUE ;
}

static JSFunctionSpec childProcessMethods_[] = {
    {"close",     jsChildProcessClose,  0 },
    {"isRunning", jsChildProcessIsRunning,  0 },
    {0}
};

static JSBool jsChildProcess( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 <= argc )
       &&
       JSVAL_IS_STRING( argv[0] )
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( cx, argv[1] ) ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsChildProcessClass_, NULL, NULL );
   
      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         JS_SetPrivate( cx, thisObj, 0 );

         JS_DefineProperty( cx, thisObj, "exitHandler", argv[1], 0, 0, JSPROP_ENUMERATE );
         JSString *sExe = JSVAL_TO_STRING( argv[0] );
         jsProcess_t *newProcess = new jsProcess_t( OBJECT_TO_JSVAL( thisObj ), 
                                                    JS_GetStringBytes( sExe ),
                                                    argc-2,
                                                    argv+2 );
         JS_SetPrivate( cx, thisObj, newProcess );

         JS_DefineProperty( cx, thisObj, "pid", INT_TO_JSVAL( newProcess->pid_ ), 0, 0, JSPROP_ENUMERATE );
      }
      else
         JS_ReportError( cx, "allocating childProcess" );
   }
   else
      JS_ReportError( cx, "Usage: childProcess( 'progName', exitHandler [,params...] )" );

   return JS_TRUE ;
}

static void cleanupProcesses( void )
{
   // send SIGKILL to all children
   {
      childProcessLock_t lock ;
      pidToChild_t const &children = getProcessMap( lock );
      for( pidToChild_t::const_iterator it = children.begin(); it != children.end(); it++ )
      {
         int const pid = (*it).first;
         if( 0 != pid )
         {
            int const result = kill( pid, SIGABRT );
            if( 0 != result )
               perror( "kill" );
         }
         else
            fprintf( stderr, "invalid pid\n" );
      }
   }
   
   //
   // now wait to reap children
   //
   do {
      { // limit scope of lock
         childProcessLock_t lock ;
         pidToChild_t const &children = getProcessMap( lock );
         if( 0 == children.size() )
            break;
      }
      pause();
   } while( 1 );
}

bool initJSProcess( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsChildProcessClass_,
                                  jsChildProcess, 1,
                                  childProcessProperties_, 
                                  childProcessMethods_, 
                                  0, 0 );
   if( rval )
   {
      startChildMonitor();
      atexit( cleanupProcesses );
      return true ;
   }
   else
      JS_ReportError( cx, "initializing childProcess class\n" );
   
   return false ;
}

void shutdownJSProcesses( void )
{
   cleanupProcesses();
}

