/*
 * Module codeQueue.cpp
 *
 * This module defines 
 *
 *
 * Change History : 
 *
 * $Log: codeQueue.cpp,v $
 * Revision 1.2  2002-10-31 02:09:38  ericn
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

typedef struct scriptAndScope_t {
   JSObject *scope_ ;
   JSScript *script_ ;
};

typedef mtQueue_t<scriptAndScope_t> codeList_t ;

static JSContext  *context_ = 0 ;
static JSObject   *global_ = 0 ;
static codeList_t  codeList_ ;

//
// returns true if compiled and queued successfully, 
// false if the code couldn't be compiled 
//
bool queueSource( JSObject          *scope,
                  std::string const &sourceCode,
                  char const        *sourceFile )
{
   JSScript *script= JS_CompileScript( context_, scope, 
                                       sourceCode.c_str(), 
                                       sourceCode.size(), 
                                       sourceFile, 1 );
   if( script )
   {
      scriptAndScope_t item ;
      item.script_ = script ;
      item.scope_  = scope ;

      if( codeList_.push( item ) )
         return true ;
      else
         fprintf( stderr, "Error queueing code\n" );
   }
   else
      fprintf( stderr, "Error compiling code %s\n", sourceCode.c_str() );

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
      script = item.script_ ;
      scope  = item.scope_ ;
      return true ;
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


