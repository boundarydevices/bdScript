/*
 * Module jsDir.cpp
 *
 * This module defines the initialization routine
 * and internals for the readDir() routine as 
 * declared and described in jsDir.h
 *
 *
 * Change History : 
 *
 * $Log: jsDir.cpp,v $
 * Revision 1.1  2003-08-31 19:31:54  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsDir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "stringVector.h"

static JSClass jsDirClass_ = {
  "dir",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};


static JSBool
jsReadDir( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sDirName = JSVAL_TO_STRING( argv[0] );
      char const *dirName = JS_GetStringBytes( sDirName );
      DIR *dir = opendir( dirName );
      if( dir )
      {
         JSObject *returnObj = JS_NewObject( cx, &jsDirClass_, NULL, NULL );
         if( returnObj )
         {
            *rval = OBJECT_TO_JSVAL( returnObj ); // root
            stringVector_t subDirs ;

            struct dirent *de ;
            while( 0 != ( de = readdir( dir ) ) )
            {
               if( ( 0 != strcmp( ".", de->d_name ) )
                   &&
                   ( 0 != strcmp( "..", de->d_name ) ) )
               {
                  char fullPath[PATH_MAX];
                  sprintf( fullPath, "%s/%s", dirName, de->d_name );
                  struct stat st ;
                  int stResult = stat( fullPath, &st );
                  if( 0 == stResult )
                  {
                     if( !S_ISLNK( st.st_mode ) )
                     {
                        if( !S_ISDIR( st.st_mode ) )
                        {
                           JSObject *fileObj = JS_NewObject( cx, &jsDirClass_, NULL, NULL );
                           if( fileObj )
                           {
                              if( !JS_DefineProperty( cx, returnObj, de->d_name, OBJECT_TO_JSVAL( fileObj ), 0, 0, JSPROP_ENUMERATE ) )
                                 JS_ReportError( cx, "Attaching %s to %s", de->d_name, dirName );

                              if( !JS_DefineProperty( cx, fileObj, "mode", INT_TO_JSVAL( st.st_mode ), 0, 0, JSPROP_ENUMERATE ) )
                                 JS_ReportError( cx, "Defining mode for %s/%s", dirName, de->d_name );
                              if( !JS_DefineProperty( cx, fileObj, "size", INT_TO_JSVAL( st.st_size ), 0, 0, JSPROP_ENUMERATE ) )
                                 JS_ReportError( cx, "Defining size for %s/%s", dirName, de->d_name );
                              if( !JS_DefineProperty( cx, fileObj, "time", INT_TO_JSVAL( st.st_mtime ), 0, 0, JSPROP_ENUMERATE ) )
                                 JS_ReportError( cx, "Defining time for %s/%s", dirName, de->d_name );
                           }
                           else
                              JS_ReportError( cx, "allocating fileObj" );
                        } // file
                        else
                        {
                           subDirs.push_back( de->d_name );
                        } // directory
                     } // skip symlinks
                     else
                     {
                        printf( "--> ignoring symlink %s/%s: mode 0x%X\n", dirName, de->d_name, st.st_mode );
                     }
                  }
                  else
                     perror( fullPath );
               }
            } // for each entry in this directory
            
            closedir( dir ); // free this dir handle
            for( unsigned i = 0 ; i < subDirs.size(); i++ )
            {
               //
               // can't attach until we have one.
               //
               jsval sSubdir = JSVAL_FALSE ;
               JS_AddRoot( cx, &sSubdir );
               char fullPath[PATH_MAX];
               int const len = sprintf( fullPath, "%s/%s", dirName, subDirs[i].c_str() );
               JSString * const sPath = JS_NewStringCopyN( cx, fullPath, len );
               jsval param = STRING_TO_JSVAL( sPath );
               JS_AddRoot( cx, &param );
               JSBool worked = jsReadDir( cx, obj, 1, &param, &sSubdir );
               if( worked )
               {
                  if( !JS_DefineProperty( cx, returnObj, subDirs[i].c_str(), sSubdir, 0, 0, JSPROP_ENUMERATE ) )
                     JS_ReportError( cx, "Attaching %s to %s", subDirs[i].c_str(), dirName );
               }
               JS_RemoveRoot( cx, &param );
               JS_RemoveRoot( cx, &sSubdir );
            }
         }
         else
         {
            JS_ReportError( cx, "allocating dirObject" );
            closedir( dir );
         }
      }
      else
         JS_ReportError( cx, "%m reading dir <%s>\n", dirName );
   }
   else
      JS_ReportError( cx, "Usage: stat( fileName )" );
   
   return JS_TRUE ;

}



static JSFunctionSpec _functions[] = {
    {"readDir",           jsReadDir,    0 },
    {0}
};

bool initJSDir( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}

