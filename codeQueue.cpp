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
 * Revision 1.8  2002-12-01 15:56:51  ericn
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
#include "mtQueue.h"
#include "jsGlobals.h"
#include "jsHyperlink.h"
#include <linux/stddef.h>

struct scriptAndScope_t {
   JSObject   *scope_ ;
   jsval       script_ ;
   char const *source_ ;
};

struct callbackAndData_t {
   callback_t callback_ ;
   void      *cbData_ ;
};

typedef mtQueue_t<callbackAndData_t> codeList_t ;

static JSContext  *context_ = 0 ;
static JSObject   *global_ = 0 ;
static codeList_t  codeList_ ;

LIST_HEAD(filters_);

void executeCode( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile )
{
   JSString *sVal ;
   if( JSVAL_IS_STRING( sourceCode ) 
       &&
       ( 0 != ( sVal = JSVAL_TO_STRING( sourceCode ) ) ) )
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
            fprintf( stderr, "error executing code\n" );
      }
      else
         JS_ReportError( context_, "compiling script from %s", sourceFile );
   }
   else
      JS_ReportError( context_, "reading script from %s", sourceFile );
}

static void runUnrootAndFree( void *scriptAndScope )
{
   scriptAndScope_t *const ss = ( scriptAndScope_t *)scriptAndScope ;
   executeCode( ss->scope_, ss->script_, ss->source_ );
   JS_RemoveRoot( context_, &ss->script_ );

   delete ss ;
}

static void runAndFree( void *scriptAndScope )
{
   scriptAndScope_t *const ss = ( scriptAndScope_t *)scriptAndScope ;
   executeCode( ss->scope_, ss->script_, ss->source_ );

   delete ss ;
}

bool queueCallback( callback_t callback,
                    void      *cbData )
{
   {
      mutexLock_t lock( codeList_.mutex_ );
      for( list_head *next = filters_.next ; next != &filters_ ; next = next->next )
      {
         printf( "checking filter %p\n", next );
         char *address = (char *)next - offsetof(codeFilter_t,chain_);
         codeFilter_t *filter = (codeFilter_t *)address ;
         if( filter->isHandled( callback, cbData ) )
         {
            if( filter->isDone() )
               codeList_.cond_.signal();
            return true ;
         }
      } // walk filter chain
   } // limit scope of lock
   
   //
   // not handled by filter, post to queue
   //
   callbackAndData_t cbd ;
   cbd.callback_ = callback ;
   cbd.cbData_   = cbData ;

   bool const worked = codeList_.push( cbd );
   if( worked )
   {
      return true ;
   }
   else
   {
      JS_ReportError( context_, "queueing callback" );
      return false ;
   }
}

bool queueUnrootedSource( JSObject   *scope,
                          jsval       sourceCode,
                          char const *sourceFile )
{
   scriptAndScope_t *newSS = new scriptAndScope_t ;
   newSS->script_ = sourceCode ;
   newSS->scope_  = scope ;
   newSS->source_ = sourceFile ;

   JS_AddRoot( context_, &newSS->script_ );
   return queueCallback( runUnrootAndFree, newSS );
}


bool queueSource( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile )
{
   scriptAndScope_t *newSS = new scriptAndScope_t ;
   newSS->script_ = sourceCode ;
   newSS->scope_  = scope ;
   newSS->source_ = sourceFile ;

   return queueCallback( runAndFree, newSS );
}

codeFilter_t :: codeFilter_t( void )
{
   mutexLock_t lock( codeList_.mutex_ );
   list_add( &chain_, &filters_ );
}

codeFilter_t :: ~codeFilter_t( void )
{
   mutexLock_t lock( codeList_.mutex_ );
   list_del( &chain_ );
}

bool codeFilter_t :: isHandled
   ( callback_t callback,
     void      *cbData )
{
   return false ;
}

void codeFilter_t :: wait( void )
{
   while( !isDone() )
   {
      mutexLock_t lock( codeList_.mutex_ );
      if( lock.worked() )
      {
         if( !codeList_.abort_ )
         {
            if( !codeList_.cond_.wait( lock ) )
               break;
         }
         else
            break; // queue abort
      }
      else
         break; // lock error
   } // until app says done
}

bool codeFilter_t :: isDone( void )
{
   return false ;
}

//
// returns true and a string full of bytecode if
// successful, false on timeout
//
void pollCodeQueue( JSContext *cx,
                    unsigned   milliseconds,
                    unsigned   iterations )
{
   context_ = cx ;

   for( unsigned i = 0 ; i < iterations ; i++ )
   {
      callbackAndData_t cbd ;
      bool const result = ( 0xFFFFFFFF == milliseconds )
                          ? codeList_.pull( cbd )
                          : codeList_.pull( cbd, milliseconds );
      if( result )
      {
         cbd.callback_( cbd.cbData_ );
         
         if( gotoCalled_ )
            break;
      }
      else
         break;
   }
}


void initializeCodeQueue
   ( JSContext *cx,
     JSObject  *glob )
{
   context_ = cx ;
   global_  = glob ;
}


