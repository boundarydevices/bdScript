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
 * Revision 1.5  2002-12-01 02:42:08  ericn
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

struct scriptAndScope_t {
   JSObject   *scope_ ;
   jsval       script_ ;
   char const *source_ ;
};

struct callbackAndData_t {
   callback_t callback_ ;
   void      *cbData_ ;
};

struct callbackOrScript_t {
   bool isCallback_ ;
   union {
      callbackAndData_t cb_ ;
      scriptAndScope_t  ss_ ;
   } u_ ;
};

typedef mtQueue_t<callbackOrScript_t> codeList_t ;

static JSContext  *context_ = 0 ;
static JSObject   *global_ = 0 ;
static codeList_t  codeList_ ;

bool queueSource( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile )
{
   callbackOrScript_t cbs ;
   cbs.isCallback_    = false ;
   cbs.u_.ss_.script_ = sourceCode ;
   cbs.u_.ss_.scope_  = scope ;
   cbs.u_.ss_.source_ = sourceFile ;

   bool const worked = codeList_.push( cbs );
   if( worked )
      return true ;
   else
      fprintf( stderr, "Error queueing code\n" );

   return false ;
}

static void runAndUnlock( void *scriptAndScope )
{
   scriptAndScope_t *const ss = ( scriptAndScope_t *)scriptAndScope ;
   
   JSString *sVal ;
   if( JSVAL_IS_STRING( ss->script_ ) 
       &&
       ( 0 != ( sVal = JSVAL_TO_STRING( ss->script_ ) ) ) )
   {
      JSScript *scr = JS_CompileScript( context_, ss->scope_, 
                                        JS_GetStringBytes( sVal ), 
                                        JS_GetStringLength( sVal ), 
                                        ss->source_, 1 );
      if( scr )
      {
         jsval rval; 
         JSBool const exec = JS_ExecuteScript( context_, ss->scope_, scr, &rval );
         JS_DestroyScript( context_, scr );
         if( !exec )
            fprintf( stderr, "error executing code\n" );
      }
      else
         JS_ReportError( context_, "compiling script from %s", ss->source_ );
   }
   else
      JS_ReportError( context_, "reading script from %s", ss->source_ );

   JS_RemoveRoot( context_, &ss->script_ );

   delete ss ;
}

bool queueCallback( callback_t callback,
                    void      *cbData )
{
   callbackOrScript_t cbs ;
   cbs.isCallback_      = true ;
   cbs.u_.cb_.callback_ = callback ;
   cbs.u_.cb_.cbData_   = cbData ;

   bool const worked = codeList_.push( cbs );
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
   return queueCallback( runAndUnlock, newSS );
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
      callbackOrScript_t cbs ;
      bool const result = ( 0xFFFFFFFF == milliseconds )
                          ? codeList_.pull( cbs )
                          : codeList_.pull( cbs, milliseconds );
      if( result )
      {
         if( cbs.isCallback_ )
         {
            cbs.u_.cb_.callback_( cbs.u_.cb_.cbData_ );
         }
         else
         {
            mutexLock_t lock( execMutex_ );

            JSString *sVal ;
            if( JSVAL_IS_STRING( cbs.u_.ss_.script_ ) 
                &&
                ( 0 != ( sVal = JSVAL_TO_STRING( cbs.u_.ss_.script_ ) ) ) )
            {
               JSScript *scr = JS_CompileScript( cx, cbs.u_.ss_.scope_, 
                                                 JS_GetStringBytes( sVal ), 
                                                 JS_GetStringLength( sVal ), 
                                                 cbs.u_.ss_.source_, 1 );
               if( scr )
               {
                  jsval rval; 
                  JSBool const exec = JS_ExecuteScript( cx, cbs.u_.ss_.scope_, scr, &rval );
                  JS_DestroyScript( cx, scr );
                  if( !exec )
                     fprintf( stderr, "error executing code\n" );
               }
               else
                  fprintf( stderr, "Compiling %s code", cbs.u_.ss_.script_ );
            }
            else
               fprintf( stderr, "Invalid script ptr\n" );
         }

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


