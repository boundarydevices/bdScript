/*
 * Module codeQueue.cpp
 *
 * This module defines 
 *
 *
 * Change History : 
 *
 * $Log: codeQueue.cpp,v $
 * Revision 1.4  2002-11-30 18:52:57  ericn
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

typedef struct scriptAndScope_t {
   JSObject   *scope_ ;
   jsval       script_ ;
   char const *source_ ;
};

typedef mtQueue_t<scriptAndScope_t> codeList_t ;

static JSContext  *context_ = 0 ;
static JSObject   *global_ = 0 ;
static codeList_t  codeList_ ;

//
// returns true if compiled and queued successfully, 
// false if the code couldn't be compiled 
//
bool queueSource( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile )
{
   scriptAndScope_t item ;
   item.script_ = sourceCode ;
   item.scope_  = scope ;
   item.source_ = sourceFile ;

   bool const worked = codeList_.push( item );
   if( worked )
      return true ;
   else
      fprintf( stderr, "Error queueing code\n" );

   return false ;
}

//
// returns true and a string full of bytecode if
// successful
//
bool dequeueByteCode( JSScript    *&script,
                      JSObject    *&scope,
                      unsigned long milliseconds )
{
   script = 0 ;
   scope = 0 ;

   scriptAndScope_t item ;
   bool const result = ( 0xFFFFFFFF == milliseconds )
                       ? codeList_.pull( item )
                       : codeList_.pull( item, milliseconds );
   if( result )
   {
      mutexLock_t lock( execMutex_ );
      JSString *sVal ;
      if( JSVAL_IS_STRING( item.script_ ) 
          &&
          ( 0 != ( sVal = JSVAL_TO_STRING( item.script_ ) ) ) )
      {
         JSScript *scr = JS_CompileScript( context_, item.scope_, 
                                           JS_GetStringBytes( sVal ), 
                                           JS_GetStringLength( sVal ), 
                                           item.source_, 1 );
         if( scr )
         {
            script = scr ;
            scope  = item.scope_ ;
            return true ;
         }
         else
            fprintf( stderr, "Compiling %s code\n", item.script_ );
      }
      else
         fprintf( stderr, "Invalid script ptr\n" );
   }
   else
      return false ;
}


void initializeCodeQueue
   ( JSContext *cx,
     JSObject  *glob )
{
   context_ = cx ;
   global_  = glob ;
}


