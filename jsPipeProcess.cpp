/*
 * Module jsPipeProcess.cpp
 *
 * This module defines the initialization routine and internal methods
 * for the pipeProcess class as declared in jsPipeProcess.h
 *
 *
 * Change History : 
 *
 * $Log: jsPipeProcess.cpp,v $
 * Revision 1.3  2009-02-09 18:52:43  ericn
 * move destruction to Finalize routine
 *
 * Revision 1.2  2008-12-28 19:54:53  ericn
 * [jsPipeProcess] Only call onExit once, watch for deletes in handlers
 *
 * Revision 1.1  2008-12-12 01:24:47  ericn
 * added pipeProcess class
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */


#include "jsPipeProcess.h"
#include "pipeProcess.h"
#include "pollHandler.h"
#include "jsGlobals.h"
#include "codeQueue.h"

// #define DEBUGPRINT
#include "debugPrint.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

extern JSClass jsPipeProcessClass_ ;

class jsPipeProcess_t ;

class pipePollHandler_t : public pollHandler_t {
public:
   pipePollHandler_t( jsPipeProcess_t &process,
                      int              pipeFd,  // actual file handle to pipe
                      int              which ); // 0 == stdin, 1 == stdout, 2 == stderr
   ~pipePollHandler_t();

   virtual void onDataAvail( void );     // POLLIN
   virtual void onWriteSpace( void );    // POLLOUT
   virtual void onError( void );         // POLLERR
   virtual void onHUP( void );           // POLLHUP

   jsPipeProcess_t &process_ ;
   int        const which_ ;
};

class jsPipeProcess_t : public pipeProcess_t {
public:
   jsPipeProcess_t( JSObject   *obj,
                    char const *args[] ); // a.la. execve
   virtual ~jsPipeProcess_t( void );

   jsval onStdin ;
   jsval onStdout ;
   jsval onStderr ;
   jsval onClose ;
   jsval onError ;
   jsval onExit ;
   
   void handleIt( jsval handler, int which ); // common handler for writeSpace(), error() and HUP()
   
   void dataAvail( int which );     // POLLIN
   void writeSpace( int which );    // POLLOUT
   void error( int which );         // POLLERR
   void HUP( int which );           // POLLHUP
   
   jsval read(JSContext *cx, JSObject *obj, int which);
   jsval write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv);

   void shutdown();
   void checkAlive();

   JSObject          *object_ ;
   pipePollHandler_t *handlers[3];
   bool               signalledExit_ ;
};

static void getHandler( JSContext  *cx,
                        JSObject   *obj, 
                        char const *name, 
                        jsval      &handler )
{
   jsval jsv ;
   if( JS_GetProperty( cx, obj, name, &jsv )
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( cx, jsv ) ) )
   {
      handler = jsv ;
   }
}

jsPipeProcess_t::jsPipeProcess_t( 
   JSObject   *obj,
   char const *args[] 
)
   : pipeProcess_t( args )
   , onStdin( JSVAL_NULL )
   , onStdout( JSVAL_NULL )
   , onStderr( JSVAL_NULL )
   , onClose( JSVAL_NULL )
   , onError( JSVAL_NULL )
   , onExit( JSVAL_NULL )
   , object_(obj)
   , signalledExit_( false )
{
   JS_AddRoot( execContext_, &object_ );
   JS_AddRoot( execContext_, &onStdin );
   JS_AddRoot( execContext_, &onStdout );
   JS_AddRoot( execContext_, &onStderr );
   JS_AddRoot( execContext_, &onClose );
   JS_AddRoot( execContext_, &onError );
   JS_AddRoot( execContext_, &onExit );

   debugPrint( "%s: pid %d, in/out/err: %d/%d/%d\n", args[0], pid(), child_stdin(), child_stdout(), child_stderr() );

   for( unsigned i = 0 ; i < 3 ; i++ ){
      handlers[i] = new pipePollHandler_t( *this, fdNum(i), i );
   }
}

jsPipeProcess_t::~jsPipeProcess_t( void )
{
   shutdown();
   if( object_ ){
      JS_RemoveRoot( execContext_, &object_ );
      JS_RemoveRoot( execContext_, &object_ );
      JS_RemoveRoot( execContext_, &onStdin );
      JS_RemoveRoot( execContext_, &onStdout );
      JS_RemoveRoot( execContext_, &onStderr );
      JS_RemoveRoot( execContext_, &onClose );
      JS_RemoveRoot( execContext_, &onError );
      JS_RemoveRoot( execContext_, &onExit );
      object_ = 0 ;
   }
}

void jsPipeProcess_t::dataAvail( int which )
{
   debugPrint( "%s: %d\n", __PRETTY_FUNCTION__, which );
   jsval appHandler = (1==which) ? onStdout : onStderr ;
   if( JSVAL_NULL != appHandler ){
      debugPrint( "have dataAvail handler\n" );
      executeCode( object_, appHandler, __PRETTY_FUNCTION__, 0, 0 );
   } else {
      pollHandler_t *handler = handlers[which];
      if( handler ){
         char inBuf[512];
         int numRead = 0 ;
         while( 0 < (numRead = ::read(handler->getFd(), inBuf, sizeof(inBuf) )) ){
            ::write( fileno(stdout), inBuf, numRead );
         }
         handler->setMask(handler->getMask()|POLLIN);
      }
   }  // no handler, purge input data
}

void jsPipeProcess_t::shutdown()
{
   for( unsigned i = 0 ; i < 3 ; i++ ){
      if( handlers[i] ){
         delete handlers[i];
         handlers[i] = 0 ;
      }
      else
         debugPrint( "%s: handler %d is gone\n", __PRETTY_FUNCTION__, i );
   }
   pipeProcess_t::shutdown();

   checkAlive();

   if( object_ ){
      JS_RemoveRoot( execContext_, &object_ );
      object_ = 0 ;
   }
   pipeProcess_t::wait();
}

void jsPipeProcess_t::handleIt( jsval handler, int which )
{
   if( JSVAL_NULL != handler ){
      jsval params[2] = {
         INT_TO_JSVAL(which),
         0
      };
      executeCode( object_, handler, __PRETTY_FUNCTION__, 1, params );
   }
   else
      debugPrint( "%s: no handler\n", __PRETTY_FUNCTION__ );
}

void jsPipeProcess_t::writeSpace( int which )
{
   debugPrint( "%s: %d\n", __PRETTY_FUNCTION__, which );
   handleIt( onStdin, which );
}

void jsPipeProcess_t::checkAlive()
{
   if( !signalledExit_ ){
      if( !isAlive() ){
         signalledExit_ = true ;
         if( JSVAL_NULL != onExit ){
            executeCode( object_, onExit, __PRETTY_FUNCTION__, 0, 0 );
            onExit = JSVAL_NULL ;
         }
         else
            debugPrint( "No pipeProcess exit handler\n" );
      }
      else
         debugPrint( "Pid %d is still alive\n", pid() );
   }
   else
      debugPrint( "%s: already exited\n", __func__ );
}

void jsPipeProcess_t::error( int which )
{
   debugPrint( "%s: %d\n", __PRETTY_FUNCTION__, which );
   if( handlers[which] ){
      handleIt( onError, which );
      if( handlers[which] ){
         handlers[which]->close();
         if( handlers[which] ){
            delete handlers[which];
            handlers[which] = 0 ;
         }
      }
      checkAlive();
   }
   else
      debugPrint( "no handler %d/%p\n", which, handlers[which] );
}

void jsPipeProcess_t::HUP( int which )
{
   debugPrint( "%s: %d\n", __PRETTY_FUNCTION__, which );
   if( handlers[which] ){
      handleIt( onClose, which );
      if( handlers[which] ){
         handlers[which]->close();
         if( handlers[which] ){
            delete handlers[which];
            handlers[which] = 0 ;
         }
      }
      checkAlive();
   }
   else
      debugPrint( "no handler %d/%p\n", which, handlers[which] );
}

jsval jsPipeProcess_t::read(JSContext *cx, JSObject *obj, int which)
{
   if( handlers[which] ){
      char inBuf[2048];
      int numRead = ::read(handlers[which]->getFd(), inBuf,sizeof(inBuf));
      if( 0 < numRead ){
         JSString *s = JS_NewStringCopyN( cx, inBuf, numRead );
         return STRING_TO_JSVAL(s);
      }
      else {
         handlers[which]->setMask(handlers[which]->getMask()|POLLIN);
         debugPrint( "re-enable read notification %d:%d:%m\n", which, numRead );
      } 
   }
   else
      JS_ReportError( cx, "Invalid handler for read(%d)\n", which );

   return JSVAL_FALSE ;
}

jsval jsPipeProcess_t::write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv)
{
   bool failed = false ;
   int totalWritten = 0 ;
   if( handlers[0] ){
      for( uintN i = 0 ; i < argc ; i++ ){
         JSString *s = JS_ValueToString( cx, argv[0] );
         if( s ){
            int numWritten = ::write(handlers[0]->getFd(), JS_GetStringBytes(s), JS_GetStringLength(s) );
            if( 0 <= numWritten ){
               totalWritten += numWritten ;
            }
            else {
               JS_ReportError( cx, "pipeProcess::write() to fd %d: %m\n", handlers[0]->getFd() );
               failed = true ;
               break;
            }
         } else {
            JS_ReportError( cx, "pipeProcess::write(): Error converting argument %d to string\n", i );
            failed = true ;
            break;
         }
      }
   }
   return failed ? JSVAL_FALSE : INT_TO_JSVAL(totalWritten);
}

pipePollHandler_t::pipePollHandler_t( 
   jsPipeProcess_t &process,
   int              pipeFd,  // actual file handle to pipe
   int              which )  // 0 == stdin, 1 == stdout, 2 == stderr
   : pollHandler_t( pipeFd, pollHandlers_ )
   , process_(process)
   , which_(which)
{
   fcntl( getFd(), F_SETFD, FD_CLOEXEC );
   fcntl( getFd(), F_SETFL, O_NONBLOCK );
   setMask( (0==which) 
               ? (POLLOUT|POLLERR|POLLHUP) 
               : (POLLIN|POLLERR|POLLHUP) );
   pollHandlers_.add( *this );
}

pipePollHandler_t::~pipePollHandler_t()
{
}


void pipePollHandler_t::onDataAvail( void )
{ 
   setMask( getMask() & ~POLLIN );
   process_.dataAvail(which_);
}

void pipePollHandler_t::onWriteSpace( void )
{ 
   setMask( getMask() & ~POLLOUT );
   process_.writeSpace(which_);
}

void pipePollHandler_t::onError( void )
{ 
   setMask( getMask() & ~POLLERR );
   debugPrint( "%s: %d\n", __PRETTY_FUNCTION__, which_ );
   process_.error(which_);
}

void pipePollHandler_t::onHUP( void )
{ 
   setMask( getMask() & ~POLLHUP );
   debugPrint( "%s: %d\n", __PRETTY_FUNCTION__, which_ );
   process_.HUP(which_);
}

static void jsPipeProcFinalize(JSContext *cx, JSObject *obj);

JSClass jsPipeProcessClass_ = {
  "pipeProcess",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsPipeProcFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec pipeProcessProperties_[] = {
  {0,0,0}
};

static JSBool
jsWrite( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsPipeProcess_t *proc = (jsPipeProcess_t *)JS_GetInstancePrivate( cx, obj, &jsPipeProcessClass_, NULL );
   if( proc )
      *rval = proc->write(cx, obj,argc,argv);
   else
      JS_ReportError( cx, "pipeProcess::write() on closed device\n" );
   return JS_TRUE ;
}

static JSBool
jsRead( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int fdNum ;
   if( (1== argc) && JSVAL_IS_INT(argv[0]) && (0 < (fdNum=JSVAL_TO_INT(argv[0]))) && (3 > fdNum) ){
      jsPipeProcess_t *proc = (jsPipeProcess_t *)JS_GetInstancePrivate( cx, obj, &jsPipeProcessClass_, NULL );
      if( proc )
         *rval = proc->read(cx, obj, fdNum);
      else
         JS_ReportError( cx, "pipeProcess::read() on closed device\n" );
   }
   else
      JS_ReportError( cx, "Usage: pipeProcess::read(1|2)\n" );
   return JS_TRUE ;
}

static JSBool
jsClose( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsPipeProcess_t *proc = (jsPipeProcess_t *)JS_GetInstancePrivate( cx, obj, &jsPipeProcessClass_, NULL );
   if( proc ){
      proc->shutdown();
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "pipeProcess::write() on closed device\n" );
   return JS_TRUE ;
}

static JSBool
jsIsAlive( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsPipeProcess_t *proc = (jsPipeProcess_t *)JS_GetInstancePrivate( cx, obj, &jsPipeProcessClass_, NULL );
   if( proc ){
      if( proc->isAlive() )
         *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "pipeProcess::write() on closed device\n" );
   return JS_TRUE ;
}

static void jsPipeProcFinalize(JSContext *cx, JSObject *obj)
{
   jsPipeProcess_t *proc = (jsPipeProcess_t *)JS_GetInstancePrivate( cx, obj, &jsPipeProcessClass_, NULL );
   if( proc ){
      debugPrint( "%s\n", __PRETTY_FUNCTION__ );
      jsval args[1] = { 
         JSVAL_VOID 
      };
      jsval rval ;
      jsClose( cx, obj, 0, args, &rval );
      JS_SetPrivate( cx, obj, 0 );
      delete proc ;
   } // have button data
}

static JSFunctionSpec pipeProcessMethods_[] = {
   { "write",           jsWrite,     0,0,0 },
   { "close",           jsClose,     0,0,0 },
   { "read",            jsRead,      0,0,0 },
   { "isAlive",         jsIsAlive,   0,0,0 },
    {0}
};

static char const *const handlerNames[] = {
   "onStdin",
   "onStdout",
   "onStderr",
   "onClose",
   "onError",
   "onExit"
};

static JSBool jsPipeProcess( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *rhObj = JSVAL_TO_OBJECT(argv[0]);
      jsval jsv ;
      JSString *progName ;
      if( JS_GetProperty( cx, rhObj, "progName", &jsv )
          &&
          JSVAL_IS_STRING(jsv)
          &&
          ( 0 != ( progName = JSVAL_TO_STRING( jsv ) ) ) )
      {
         char const **args = 0 ;
         JSObject  *argObj = 0 ;
         JSIdArray *argElements = 0 ;
         if( JS_GetProperty( cx, rhObj, "args", &jsv )
             &&
             JSVAL_IS_OBJECT(jsv)
             &&
             ( 0 != ( argObj = JSVAL_TO_OBJECT( jsv ) ) ) 
             &&
             ( 0 != ( argElements = JS_Enumerate( cx, argObj ) ) ) ){
            args = new char const *[argElements->length+2];
            args[0] = JS_GetStringBytes(progName);
            args[argElements->length+1] = 0 ;
            bool failed = false ;
            for( int i = 0 ; i < argElements->length ; i++ ){
               JSString *sArg ;
               if( JS_LookupElement( cx, argObj, i, &jsv ) // look up each element
                   &&
                   ( 0 != (sArg = JS_ValueToString(cx, jsv) ) ) ) // and convert to string
               {
                  args[i+1] = JS_GetStringBytes(sArg);
               }
               else {
                  JS_ReportError(cx, "Error converting args[%d] to string\n", i );
                  failed = true ;
                  break;
               }
            }
            JS_DestroyIdArray( cx, argElements );
            if( failed ){
               delete [] args ;
               return JS_TRUE ;
            }
         } // have arguments
         else {
            args = new char const *[2];
            args[0] = JS_GetStringBytes(progName);
            args[1] = 0 ;
         }

         JSObject *thisObj = JS_NewObject( cx, &jsPipeProcessClass_, NULL, NULL );
         if( thisObj )
         {
            *rval = OBJECT_TO_JSVAL( thisObj ); // root
            jsPipeProcess_t *proc = new jsPipeProcess_t(thisObj, args);
            jsval *values[] = {
               &proc->onStdin,
               &proc->onStdout,
               &proc->onStderr,
               &proc->onClose,
               &proc->onError,
               &proc->onExit
            };
            for( unsigned i = 0 ; i < sizeof(handlerNames)/sizeof(handlerNames[0]); i++ ){
               getHandler( cx, rhObj, handlerNames[i], *values[i] );
            }
            JS_SetPrivate( cx, thisObj, proc );
         }
         else
            JS_ReportError( cx, "allocating pipeProcess" );

         delete [] args ;
      } else {
         JS_ReportError( cx, "Missing required progName parameter" );
      }
   }
   else
      JS_ReportError( cx, "Usage: pipeProcess( { progName: '/path/to/exe', ... } )" );
   return JS_TRUE ;
}


bool initJSPipeProcess( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsPipeProcessClass_,
                                  jsPipeProcess, 1,
                                  pipeProcessProperties_, 
                                  pipeProcessMethods_, 
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      JS_ReportError( cx, "initializing childProcess class\n" );
   
   return false ;
}
