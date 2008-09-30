/*
 * Module codeQueue.cpp
 *
 * This module defines the code-queue pulling 
 * routines. 
 *
 *
 * Change History : 
 *
 * $Log: codeQueue.cpp,v $
 * Revision 1.24  2008-09-30 23:22:40  ericn
 * stop using offsetof so compiler stays happy
 *
 * Revision 1.23  2006-12-01 18:33:33  tkisky
 * -include assert.h
 *
 * Revision 1.22  2005/11/05 20:22:04  ericn
 * -fix signed/unsigned test
 *
 * Revision 1.21  2004/12/28 03:32:57  ericn
 * -localize filter vars
 *
 * Revision 1.20  2004/01/01 20:11:12  ericn
 * -report errors only if not shutting down
 *
 * Revision 1.19  2004/01/01 16:02:39  ericn
 * -added error message
 *
 * Revision 1.18  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.17  2003/11/22 21:02:37  ericn
 * -made code queue a pollHandler_t
 *
 * Revision 1.16  2003/11/22 20:20:41  ericn
 * -use pipe for code queue
 *
 * Revision 1.15  2003/09/05 13:04:44  ericn
 * -made pollCodeQueue() return bool (not timed out), added exec and exit tests
 *
 * Revision 1.14  2003/07/06 01:21:41  ericn
 * -added method abortCodeQueue()
 *
 * Revision 1.13  2003/06/10 23:53:31  ericn
 * -made error message more explicit
 *
 * Revision 1.12  2003/06/08 15:20:02  ericn
 * -added support for queued function call
 *
 * Revision 1.11  2003/01/06 04:30:29  ericn
 * -modified to allow unlink() of filter before destruction
 *
 * Revision 1.10  2002/12/15 20:02:22  ericn
 * -made error message more specific
 *
 * Revision 1.9  2002/12/02 15:16:10  ericn
 * -removed use of codeQueue's mutex and event by filter chain
 *
 * Revision 1.8  2002/12/01 15:56:51  ericn
 * -added filter support
 *
 * Revision 1.7  2002/12/01 14:58:51  ericn
 * -changed queue to only deal with callbacks
 *
 * Revision 1.6  2002/12/01 03:14:42  ericn
 * -added executeCode() method for handlers
 *
 * Revision 1.5  2002/12/01 02:42:08  ericn
 * -added queueCallback() and queueUnrootedSource(), changed dequeueByteCode() to pollCodeQueue()
 *
 * Revision 1.4  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.3  2002/11/17 16:09:22  ericn
 * -fixed non-rooted JSScript bug
 *
 * Revision 1.2  2002/10/31 02:09:38  ericn
 * -added scope to code queue
 *
 * Revision 1.1  2002/10/27 17:42:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "codeQueue.h"
#include "jsGlobals.h"
#include "jsHyperlink.h"
#include "jsExit.h"
#include <linux/stddef.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <assert.h>

#define MAXARGS 8

struct scriptAndScope_t {
   JSObject   *scope_ ;
   jsval       script_ ;
   char const *source_ ;
   unsigned    argc_ ;
   jsval       argv_[MAXARGS];
};

struct callbackAndData_t {
   callback_t callback_ ;
   void      *cbData_ ;
};

static JSContext  *context_ = 0 ;
static JSObject   *global_ = 0 ;
static int         codeListRead_ = -1 ;
static int         codeListWrite_ = -1 ;

static pthread_mutex_t filterMutex_ = PTHREAD_MUTEX_INITIALIZER ; 
static pthread_cond_t  filterCond_ = PTHREAD_COND_INITIALIZER ;

LIST_HEAD(filters_);

static char const *const jsTypeNames_[] = {
    "JSTYPE_VOID",                /* undefined */
    "JSTYPE_OBJECT",              /* object */
    "JSTYPE_FUNCTION",            /* function */
    "JSTYPE_STRING",              /* string */
    "JSTYPE_NUMBER",              /* number */
    "JSTYPE_BOOLEAN",             /* boolean */
    "JSTYPE_LIMIT"
};

void executeCode( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile,
                  unsigned    argc,
                  jsval      *argv )
{
   JSType const jst = JS_TypeOfValue( context_, sourceCode );
   switch( jst )
   {
      case JSTYPE_STRING:
         {
            JSString *sVal = JSVAL_TO_STRING( sourceCode );
            if( 0 != sVal )
            {
               JSScript *scr = JS_CompileScript( context_, scope, 
                                                 JS_GetStringBytes( sVal ), 
                                                 JS_GetStringLength( sVal ), 
                                                 sourceFile, 1 );
               if( scr )
               {
                  jsval rval; 
                  JSBool const exec = JS_ExecuteScript( context_, scope, scr, &rval );
                  JS_DestroyScript( context_, scr );
                  if( !exec )
                  {
                     fprintf( stderr, "error executing code<" );
                     fwrite( JS_GetStringBytes( sVal ), JS_GetStringLength( sVal ), 1, stderr );
                     fprintf( stderr, ">\n" );
                  }
               }
               else
                  JS_ReportError( context_, "compiling script from %s", sourceFile );
            }
            else
               JS_ReportError( context_, "reading script from %s", sourceFile );
            break;
         }
      case JSTYPE_FUNCTION:
         {
            JSFunction *func = JS_ValueToFunction( context_, sourceCode );
            if( func )
            {
               jsval rval; 
               if( !JS_CallFunction( context_, scope, func, argc, argv, &rval ) )
                  JS_ReportError( context_, "calling function from %s\n", sourceFile );
            }
            else
               JS_ReportError( context_, "getting function from %s\n", sourceFile );
            break;
         }
      default:
         {
            JS_ReportError( context_, "Don't know how to execute type %d/%s\n", 
                            jst, 
                            (jst< (int)((sizeof(jsTypeNames_)/sizeof(jsTypeNames_[0]))))
                            ? jsTypeNames_[jst] : "undefined" );
         }
   }
}

static void runUnrootAndFree( void *scriptAndScope )
{
   scriptAndScope_t *const ss = ( scriptAndScope_t *)scriptAndScope ;
   executeCode( ss->scope_, ss->script_, ss->source_, ss->argc_, ss->argv_ );
   JS_RemoveRoot( context_, &ss->script_ );
   for( unsigned i = 0 ; i < ss->argc_ ; i++ )
      JS_RemoveRoot( context_, &ss->argv_[i] );

   delete ss ;
}

static void runAndFree( void *scriptAndScope )
{
   scriptAndScope_t *const ss = ( scriptAndScope_t *)scriptAndScope ;
   executeCode( ss->scope_, ss->script_, ss->source_, ss->argc_, ss->argv_ );
   for( unsigned i = 0 ; i < ss->argc_ ; i++ )
      JS_RemoveRoot( context_, &ss->argv_[i] );

   delete ss ;
}

bool queueCallback( callback_t callback,
                    void      *cbData )
{
   if( 0 == pthread_mutex_lock( &filterMutex_ ) )
   {
      for( list_head *next = filters_.next ; next != &filters_ ; next = next->next )
      {
         codeFilter_t *filter = (codeFilter_t *)next ;
         unsigned offset = ((char *)&filter->chain_) - (char *)filter ;
         char *address = (char *)next - offset ;
         filter = (codeFilter_t *)address ;
         if( filter->isHandled( callback, cbData ) )
         {
            if( filter->isDone() )
            {
               pthread_cond_signal( &filterCond_ );
            }
            pthread_mutex_unlock( &filterMutex_ );
            return true ;
         }
      } // walk filter chain
      pthread_mutex_unlock( &filterMutex_ );
   }
   else
      fprintf( stderr, "Error locking filter mutex in callback\n" );
   
   //
   // not handled by filter, post to queue
   //
   callbackAndData_t cbd ;
   cbd.callback_ = callback ;
   cbd.cbData_   = cbData ;

   bool const worked = (sizeof(cbd) == write( codeListWrite_, &cbd, sizeof(cbd) ) );
   if( worked )
   {
      return true ;
   }
   else 
   {
      if( -1 != codeListWrite_ )
      {
         perror( "queueCallback" );
         JS_ReportError( context_, "queueing callback" );
      }
      return false ;
   }
}

bool queueUnrootedSource( JSObject   *scope,
                          jsval       sourceCode,
                          char const *sourceFile,
                          unsigned    argc,
                          jsval      *argv )
{
   scriptAndScope_t *newSS = new scriptAndScope_t ;
   newSS->script_ = sourceCode ;
   newSS->scope_  = scope ;
   newSS->source_ = sourceFile ;
   newSS->argc_   = argc ;
   if( argc > MAXARGS )
      argc = MAXARGS ;
   memcpy( newSS->argv_, argv, argc*sizeof(argv[0]) );
   JS_AddRoot( context_, &newSS->script_ );
   for( unsigned i = 0 ; i < argc ; i++ )
      JS_AddRoot( context_, &newSS->argv_[i] );
   return queueCallback( runUnrootAndFree, newSS );
}


bool queueSource( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile,
                  unsigned    argc,
                  jsval      *argv )
{
   scriptAndScope_t *newSS = new scriptAndScope_t ;
   newSS->script_ = sourceCode ;
   newSS->scope_  = scope ;
   newSS->source_ = sourceFile ;
   if( argc > MAXARGS )
      argc = MAXARGS ;
   newSS->argc_   = argc ;
   memcpy( newSS->argv_, argv, argc*sizeof(argv[0]) );
   for( unsigned i = 0 ; i < argc ; i++ )
      JS_AddRoot( context_, &newSS->argv_[i] );

   return queueCallback( runAndFree, newSS );
}

codeFilter_t :: codeFilter_t( void )
{
   if( 0 == pthread_mutex_lock( &filterMutex_ ) )
   {
      assert( list_empty( &filters_ ) ); // only JS thread can install a filter, and only one
      list_add( &chain_, &filters_ );
      pthread_mutex_unlock( &filterMutex_ );
   }
   else
      fprintf( stderr, "Error locking filter mutex\n" );
}

codeFilter_t :: ~codeFilter_t( void )
{
   unlink();
}

bool codeFilter_t :: isHandled
   ( callback_t callback,
     void      *cbData )
{
   return false ;
}

void codeFilter_t :: wait( void )
{
   if( 0 == pthread_mutex_lock( &filterMutex_ ) )
   {
      while( !isDone() )
      {
         pthread_cond_wait( &filterCond_, &filterMutex_ );
      } // until app says done

      pthread_mutex_unlock( &filterMutex_ );
   }
   else
      fprintf( stderr, "Error locking filter mutex for wait\n" );
}

bool codeFilter_t :: isDone( void )
{
   return false ;
}

void codeFilter_t :: unlink( void )
{
   if( 0 == pthread_mutex_lock( &filterMutex_ ) )
   {
      if( !list_empty( &chain_ ) )
      {
         list_del( &chain_ );
         INIT_LIST_HEAD( &chain_ );
      }
      pthread_mutex_unlock( &filterMutex_ );
   }
   else
      fprintf( stderr, "Error locking filter mutex\n" );
}

void abortCodeQueue( void )
{
   int const fds[2] = {
      codeListRead_, codeListWrite_
   };
   codeListRead_ = codeListWrite_ = -1 ;
   close( fds[0] );
   close( fds[1] );
}


class codeQueueHandler_t : public pollHandler_t {
public:
   codeQueueHandler_t( int readFd, pollHandlerSet_t &set )
      : pollHandler_t( readFd, set )
   {
      setMask( POLLIN );
   }

   //
   // These routines should return true if the handler
   // still wants callbacks on the same events.
   //
   virtual void onDataAvail( void );     // POLLIN
};

void codeQueueHandler_t::onDataAvail( void )
{
   callbackAndData_t cbd ;
   int const numRead = read( fd_, &cbd, sizeof( cbd ) );
   if( ( sizeof(cbd) == numRead ) && ( 0 != cbd.callback_ ) )
   {
      mutexLock_t lock( execMutex_ );
      cbd.callback_( cbd.cbData_ );
   }
}

void initializeCodeQueue
   ( pollHandlerSet_t &pollSet,
     JSContext        *cx,
     JSObject         *glob )
{
   int fds[2];
   if( 0 == pipe( fds ) )
   {
      fcntl(fds[0], F_SETFL, FD_CLOEXEC);
      fcntl(fds[1], F_SETFL, FD_CLOEXEC);
      codeListRead_  = fds[0];
      fcntl(codeListRead_, F_SETFL, O_NONBLOCK);
      codeListWrite_ = fds[1];
      context_ = cx ;
      global_  = glob ;
      codeQueueHandler_t *const handler = new codeQueueHandler_t( codeListRead_, pollSet );
      pollSet.add( *handler );
   }
   else
   {
      perror( "codePipe" );
      exit( 1 );
   }
}


