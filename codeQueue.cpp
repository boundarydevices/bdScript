/*
 * Module codeQueue.cpp
 *
 * This module defines 
 *
 *
 * Change History : 
 *
 * $Log: codeQueue.cpp,v $
 * Revision 1.1  2002-10-27 17:42:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "codeQueue.h"
#include "mtQueue.h"

typedef mtQueue_t<JSScript *> codeList_t ;

static JSContext  *context_ = 0 ;
static JSObject   *global_ = 0 ;
static codeList_t  codeList_ ;

//
// returns true if compiled and queued successfully, 
// false if the code couldn't be compiled 
//
bool queueSource( std::string const &sourceCode,
                  char const        *sourceFile )
{
   JSScript *script= JS_CompileScript( context_, global_, 
                                       sourceCode.c_str(), 
                                       sourceCode.size(), 
                                       sourceFile, 1 );
   if( script )
   {
      if( codeList_.push( script ) )
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
JSScript *dequeueByteCode( unsigned long milliseconds )
{
   JSScript *code ;
   bool const result = ( 0xFFFFFFFF == milliseconds )
                       ? codeList_.pull( code )
                       : codeList_.pull( code, milliseconds );
   if( result )
      return code ;
   else
      return 0 ;
}


void initializeCodeQueue
   ( JSContext *cx,
     JSObject  *glob )
{
   context_ = cx ;
   global_  = glob ;
}


