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
 * Revision 1.3  2003-08-31 16:52:46  ericn
 * -added optional timestamp to writeFile, added touch() routine
 *
 * Revision 1.2  2003/08/31 15:06:32  ericn
 * -added method stat
 *
 * Revision 1.1  2003/03/04 14:45:18  ericn
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
#include <sys/stat.h>
#include <utime.h>

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
   if( ( 2 <= argc )
       &&
       ( 3 >= argc )
       &&
       JSVAL_IS_STRING( argv[0] ) 
       &&
       JSVAL_IS_STRING( argv[1] ) 
       &&
       ( ( 2 == argc ) 
         ||
         ( JSVAL_IS_INT( argv[2] ) ) ) )
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
         if( ( *rval == JSVAL_TRUE ) && ( 3 == argc ) )
         {
            utimbuf tb ;
            tb.actime = tb.modtime = JSVAL_TO_INT( argv[2] );
            utime( fileName, &tb );
         } // set timestamp
      }
      else
         JS_ReportError( cx, "%s opening %s for write", strerror( errno ), fileName );
   }
   else
      JS_ReportError( cx, "Usage: writeFile( fileName, data [,timestamp] )" );
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

static JSClass jsStatClass_ = {
  "stat",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};


static JSBool
jsStat( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sFileName = JSVAL_TO_STRING( argv[0] );
      struct stat st ;
      int const stResult = stat( JS_GetStringBytes( sFileName ), &st );
      if( 0 == stResult )
      {
         JSObject *returnObj = JS_NewObject( cx, &jsStatClass_, NULL, NULL );
         if( returnObj )
         {
            *rval = OBJECT_TO_JSVAL( returnObj ); // root
   
            JS_DefineProperty( cx, returnObj, "dev",     INT_TO_JSVAL( st.st_dev     ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "ino",     INT_TO_JSVAL( st.st_ino     ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "mode",    INT_TO_JSVAL( st.st_mode    ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "nlink",   INT_TO_JSVAL( st.st_nlink   ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "uid",     INT_TO_JSVAL( st.st_uid     ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "gid",     INT_TO_JSVAL( st.st_gid     ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "rdev",    INT_TO_JSVAL( st.st_rdev    ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "size",    INT_TO_JSVAL( st.st_size    ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "blksize", INT_TO_JSVAL( st.st_blksize ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "blocks",  INT_TO_JSVAL( st.st_blocks  ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "atime",   INT_TO_JSVAL( st.st_atime   ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "mtime",   INT_TO_JSVAL( st.st_mtime   ), 0, 0, JSPROP_ENUMERATE );
            JS_DefineProperty( cx, returnObj, "ctime",   INT_TO_JSVAL( st.st_ctime   ), 0, 0, JSPROP_ENUMERATE );
         }
         else
            JS_ReportError( cx, "Allocating stat object" );
      }
      else
      {
         *rval = STRING_TO_JSVAL( JS_NewStringCopyZ( cx, strerror( errno ) ) );
      }
   }
   else
      JS_ReportError( cx, "Usage: stat( fileName )" );
   
   return JS_TRUE ;

}

static JSBool
jsTouch( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 2 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) 
       &&
       JSVAL_IS_INT( argv[1] ) )
   {
      JSString *sFileName = JSVAL_TO_STRING( argv[0] );
      char const *fileName = JS_GetStringBytes( sFileName );

      utimbuf tb ;
      tb.actime = tb.modtime = JSVAL_TO_INT( argv[1] );
      int const result = utime( fileName, &tb );
      if( 0 == result )
      {
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "%m setting timestamp for %s\n", fileName );
   }
   else
      JS_ReportError( cx, "Usage: stat( fileName )" );
   
   return JS_TRUE ;

}

static JSFunctionSpec _functions[] = {
    {"readFile",           jsReadFile,    0 },
    {"writeFile",          jsWriteFile,   0 },
    {"unlink",             jsUnlink,      0 },
    {"copyFile",           jsCopyFile,    0 },
    {"renameFile",         jsRenameFile,  0 },
    {"stat",               jsStat,        0 },
    {"touch",              jsTouch,       0 },
    {0}
};

 
bool initJSFileIO( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}


