/*
 * Module jsFileIO.cpp
 *
 * This module implements the Javascript File I/O wrappers
 * as described in jsFileIO.h
 *
 *
 *
 * Change History : 
 *
 * $Log: jsFileIO.cpp,v $
 * Revision 1.1  2003-03-04 14:45:18  ericn
 * -added jsFileIO module
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsFileIO.h"
#include <stdio.h>
#include <stdlib.h>
#include "memFile.h"
#include <string.h>
#include <errno.h>
#include <unistd.h>

/*    readFile( filename );   - returns string with file content
 *    writeFile( filename, data ); - writes data to filename, returns bool or errorMsg
 *    unlink( filename );
 *    copyFile( srcfile, destfile );
 *    renameFile( srcfile, destfile );
 */

static JSBool
jsReadFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *jsPath = JSVAL_TO_STRING( argv[0] );
      char const *fileName = JS_GetStringBytes( jsPath );
      memFile_t fIn( fileName );
      if( fIn.worked() )
      {
         JSString *result = JS_NewStringCopyN( cx, (char const *)fIn.getData(), fIn.getLength() );
         *rval = STRING_TO_JSVAL( result );
      }
      else
      {
         JS_ReportError( cx, "%s reading %s", strerror( errno ), fileName );
      }
   }
   else
      JS_ReportError( cx, "Usage: readFile( fileName )" );
   return JS_TRUE ;
}

static JSBool
jsWriteFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) 
       &&
       JSVAL_IS_STRING( argv[1] ) )
   {
      JSString *jsPath = JSVAL_TO_STRING( argv[0] );
      char const *fileName = JS_GetStringBytes( jsPath );
      FILE *fOut = fopen( fileName, "wb" );
      if( fOut )
      {
         JSString *jsData = JSVAL_TO_STRING( argv[1] );
         unsigned const len = JS_GetStringLength( jsData );
         int numWritten = fwrite( JS_GetStringBytes( jsData ), 1, len, fOut );

         if( (unsigned)numWritten == len )
            *rval = JSVAL_TRUE ;
         else
            JS_ReportError( cx, "%m writing to %s", fileName );
         
         fclose( fOut );
      }
      else
         JS_ReportError( cx, "%s opening %s for write", strerror( errno ), fileName );
   }
   else
      JS_ReportError( cx, "Usage: writeFile( fileName, data )" );
   return JS_TRUE ;
}

static JSBool
jsUnlink( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *jsPath = JSVAL_TO_STRING( argv[0] );
      char const *fileName = JS_GetStringBytes( jsPath );
      int result = unlink( fileName );
      if( 0 == result )
      {
         *rval = JSVAL_TRUE ;
      }
      else
      {
         JS_ReportError( cx, "%s removing %s", strerror( errno ), fileName );
      }
   }
   else
      JS_ReportError( cx, "Usage: unlink( fileName )" );
   return JS_TRUE ;
}

static JSBool
jsCopyFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) 
       &&
       JSVAL_IS_STRING( argv[1] ) )
   {
      JSString *jsSrcFile = JSVAL_TO_STRING( argv[0] );
      char const *srcFile = JS_GetStringBytes( jsSrcFile );
      memFile_t fIn( srcFile );
      if( fIn.worked() )
      {
         JSString *jsDestFile = JSVAL_TO_STRING( argv[1] );
         char const *destFile = JS_GetStringBytes( jsDestFile );
         FILE *fOut = fopen( destFile, "wb" );
         if( fOut )
         {
            int numWritten = fwrite( fIn.getData(), 1, fIn.getLength(), fOut );
   
            if( fIn.getLength() == (unsigned)numWritten )
               *rval = JSVAL_TRUE ;
            else
               JS_ReportError( cx, "%m writing to %s", destFile );

            fclose( fOut );
         }
         else
            JS_ReportError( cx, "%s opening %s for write", strerror( errno ), destFile );
      }
      else
      {
         JS_ReportError( cx, "%s reading %s", strerror( errno ), srcFile );
      }
   }
   else
      JS_ReportError( cx, "Usage: rename( srcFileName, destFileName )" );
   
   return JS_TRUE ;
}

static JSBool
jsRenameFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) 
       &&
       JSVAL_IS_STRING( argv[1] ) )
   {
      JSString *jsSrcFile = JSVAL_TO_STRING( argv[0] );
      char const *srcFile = JS_GetStringBytes( jsSrcFile );
      JSString *jsDestFile = JSVAL_TO_STRING( argv[1] );
      char const *destFile = JS_GetStringBytes( jsDestFile );
      int result = rename( srcFile, destFile );
      if( 0 == result )
      {
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "%s renaming %s => %s", strerror( errno ), srcFile, destFile );
   }
   else
      JS_ReportError( cx, "Usage: rename( srcFileName, destFileName )" );
   
   return JS_TRUE ;

}


static JSFunctionSpec _functions[] = {
    {"readFile",           jsReadFile,    0 },
    {"writeFile",          jsWriteFile,   0 },
    {"unlink",             jsUnlink,      0 },
    {"copyFile",           jsCopyFile,    0 },
    {"renameFile",         jsRenameFile,  0 },
    {0}
};

 
bool initJSFileIO( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}


