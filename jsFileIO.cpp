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
 * Revision 1.4  2004-04-20 15:18:10  ericn
 * -Added file class (for devices)
 *
 * Revision 1.3  2003/08/31 16:52:46  ericn
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
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "pollHandler.h"
#include "stringList.h"
#include "jsGlobals.h"

static JSObject *fileProto = NULL ;

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

 
class filePollHandler_t : public pollHandler_t {
public:
   filePollHandler_t( int               fd, 
                      pollHandlerSet_t &set,
                      JSContext        *cx );
   ~filePollHandler_t( void );

   void setCharIn( jsval handler, jsval scope );
   void setLineIn( jsval handler, jsval scope );


   virtual void onDataAvail( void );     // POLLIN
   virtual void onLineIn( void );
   virtual void onCharIn( void );

   inline char const *getLine( void ) const { return dataBuf_ ; }
   inline char getTerm( void ) const { return terminator_ ; }

protected:
   char           dataBuf_[512];
   unsigned       numRead_ ;
   char           terminator_ ;
   jsval          onLineInCode_ ;
   jsval          onLineInScope_ ;
   jsval          onCharInCode_ ;
   jsval          onCharInScope_ ;
   JSContext *    cx_ ;
};

filePollHandler_t :: filePollHandler_t
   ( int               fd, 
     pollHandlerSet_t &set,
     JSContext        *cx )
   : pollHandler_t( fd, set )
   , numRead_( 0 )
   , terminator_( 0 )
   , onLineInCode_( JSVAL_VOID )
   , onLineInScope_( JSVAL_VOID )
   , onCharInCode_( JSVAL_VOID )
   , onCharInScope_( JSVAL_VOID )
   , cx_( cx )
{
   JS_AddRoot( cx, &onLineInCode_ );
   JS_AddRoot( cx, &onLineInScope_ );
   JS_AddRoot( cx, &onCharInCode_ );
   JS_AddRoot( cx, &onCharInScope_ );
   
   dataBuf_[0] = '\0' ;
   fcntl( getFd(), F_SETFD, FD_CLOEXEC );
   setMask( 0 );
   set.add( *this );
}

filePollHandler_t :: ~filePollHandler_t( void )
{
   JS_RemoveRoot( cx_, &onLineInCode_ );
   JS_RemoveRoot( cx_, &onLineInScope_ );
   JS_RemoveRoot( cx_, &onCharInCode_ );
   JS_RemoveRoot( cx_, &onCharInScope_ );
}

void filePollHandler_t :: setCharIn( jsval handler, jsval scope )
{
   onCharInCode_  = handler ;
   onCharInScope_ = scope ;
   if( ( JSVAL_FALSE != onCharInCode_ ) || ( JSVAL_FALSE != onLineInCode_ ) )
      setMask( POLLIN );
   else
      setMask( 0 );
}

void filePollHandler_t :: setLineIn( jsval handler, jsval scope )
{
   onLineInCode_  = handler ;
   onLineInScope_ = scope ;
   if( ( JSVAL_FALSE != onCharInCode_ ) || ( JSVAL_FALSE != onLineInCode_ ) )
      setMask( POLLIN );
   else
      setMask( 0 );
}

void filePollHandler_t :: onDataAvail( void )
{

   int numAvail = 32 ;
   if( 0 == ioctl( getFd(), FIONREAD, &numAvail ) )
   {
      for( int i = 0 ; i < numAvail ; i++ )
      {
         char c ;
         if( 1 == read( getFd(), &c, 1 ) )
         {
            if( ( '\r' == c ) 
                || 
                ( '\n' == c ) )
            {
               terminator_ = c ;
               onLineIn();
               numRead_ = 0 ;
               dataBuf_[numRead_] = '\0' ;
            }
            else if( ( '\x1b' == c )
                     || 
                     ( '\x03' == c ) )
            {
               terminator_ = c ;
               numRead_ = 0 ;
               dataBuf_[numRead_] = '\0' ;
               onLineIn();
            }
            else if( '\x15' == c )     // ctrl-u
            {
               numRead_ = 0 ;
               dataBuf_[numRead_] = '\0' ;
            }
            else if( '\b' == c )
            {
               if( 0 < numRead_ )
               {
                  dataBuf_[--numRead_] = '\0' ;
               }
            }
            else if( numRead_ < ( sizeof( dataBuf_ ) - 1 ) )
            {
               dataBuf_[numRead_++] = c ;
               dataBuf_[numRead_] = '\0' ;
            }
         }
         else
            break;
      }
      
      if( ( 0 < numRead_ ) && ( JSVAL_VOID != onCharInCode_ ) )
      {
         onCharIn();
         numRead_ = 0 ;
         dataBuf_[numRead_] = '\0' ;
      } // have charIn handler
/*
*/   
   }
   else
   {
      perror( "FIONREAD" );
      setMask( 0 );
   }
/*
*/   
}

void filePollHandler_t :: onLineIn( void )
{
   printf( "linein<%s>, length %u, terminator <%02x>\n", getLine(), strlen( getLine() ), getTerm() );
}

void filePollHandler_t :: onCharIn( void )
{
   printf( "charin<%s>, length %u, terminator <%02x>\n", getLine(), strlen( getLine() ), getTerm() );
}

static void jsFileFinalize(JSContext *cx, JSObject *obj);

JSClass jsFileClass_ = {
  "file",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsFileFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec fileProperties_[] = {
  {0,0,0}
};

static void jsFileFinalize(JSContext *cx, JSObject *obj)
{
   filePollHandler_t * const handler = (filePollHandler_t *)JS_GetInstancePrivate( cx, obj, &jsFileClass_, NULL );
   if( 0 != handler )
   {
      JS_SetPrivate( cx, obj, 0 );
      handler->close();
      delete handler ;
   }
}

static int modeFromString( char const *sMode )
{
   char const *startMode = sMode ;
   
   bool read = false ; bool write = false ;
   while( *sMode )
   {
      char const c = tolower( *sMode );
      sMode++ ;
      switch( c )
      {
         case 'r' :  read = true ; break ;
         case 'w' :  write = true ; break ;
         default:
            printf( "unknown mode string char : %c\n", c );
      }
   }
   int mode = -1 ;
   if( read )
   {
      if( write )
         mode = O_RDWR ;
      else
         mode = O_RDONLY ;
   }
   else if( write )
   {
      mode = O_WRONLY ;
   }
   else
      printf( "Invalid mode string %s for file\n", startMode );

   mode |= O_NONBLOCK ;

   return mode ;
}

static JSBool jsFileIsOpen( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   filePollHandler_t * const handler = (filePollHandler_t *)JS_GetInstancePrivate( cx, obj, &jsFileClass_, NULL );
   if( ( 0 != handler ) && handler->isOpen() )
      *rval = JSVAL_TRUE ;

   return JS_TRUE ;
}

static JSBool jsFileClose( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   filePollHandler_t * const handler = (filePollHandler_t *)JS_GetInstancePrivate( cx, obj, &jsFileClass_, NULL );
   if( ( 0 != handler ) && handler->isOpen() )
   {
      handler->close();
      *rval = JSVAL_TRUE ;
   }

   return JS_TRUE ;
}

static JSBool jsFileOnCharIn( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsval vobj = JSVAL_FALSE ;
   jsval vhandler = JSVAL_FALSE ;
   
   if( ( 1 <= argc )
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( cx, argv[0] ) ) )
   {
      vhandler = argv[0];
      if( ( 2 == argc )
          &&
          JSVAL_IS_OBJECT( argv[1] ) ) 
         vobj = argv[1];
   }
   else if( 0 == argc )
   {
   }  // clearing handler
   else
      JS_ReportError( cx, "Usage: file.onCharIn( function, obj ) or file.onCharIn()\n" );

   filePollHandler_t * const handler = (filePollHandler_t *)JS_GetInstancePrivate( cx, obj, &jsFileClass_, NULL );
   if( handler )
   {
      handler->setCharIn( vhandler, vobj );
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "file is uninitialized\n" );
   
   return JS_TRUE ;
}

static JSBool jsFileOnLineIn( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsval vobj = JSVAL_FALSE ;
   jsval vhandler = JSVAL_FALSE ;
   
   if( ( 1 <= argc )
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( cx, argv[0] ) ) )
   {
      vhandler = argv[0];
      if( ( 2 == argc )
          &&
          JSVAL_IS_OBJECT( argv[1] ) ) 
         vobj = argv[1];
   }
   else if( 0 == argc )
   {
   }  // clearing handler
   else
      JS_ReportError( cx, "Usage: file.onLineIn( function, obj ) or file.onLineIn()\n" );

   filePollHandler_t * const handler = (filePollHandler_t *)JS_GetInstancePrivate( cx, obj, &jsFileClass_, NULL );
   if( handler )
   {
      handler->setLineIn( vhandler, vobj );
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "file is uninitialized\n" );
   
   return JS_TRUE ;
}

static JSBool jsFileRead( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( 0 == argc )
   {
      filePollHandler_t * const handler = (filePollHandler_t *)JS_GetInstancePrivate( cx, obj, &jsFileClass_, NULL );
      if( handler )
      {
         char const *data = handler->getLine();
         unsigned const len = strlen( data );
         JSString *sData = JS_NewStringCopyN( cx, data, len );
         *rval = STRING_TO_JSVAL( sData );
      }
      else
         JS_ReportError( cx, "file is uninitialized\n" );
   }
   else
      JS_ReportError( cx, "Usage: file.read();" );

   return JS_TRUE ;
}

static JSBool jsFileWrite( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sParam = JSVAL_TO_STRING( argv[0] );
      char const *param = JS_GetStringBytes( sParam );
      unsigned const len = JS_GetStringLength( sParam );
      filePollHandler_t * const handler = (filePollHandler_t *)JS_GetInstancePrivate( cx, obj, &jsFileClass_, NULL );
      if( handler && handler->isOpen() )
      {
         int const numWritten = write( handler->getFd(), param, len );
         *rval = INT_TO_JSVAL( numWritten );
      }
      else
         JS_ReportError( cx, "file is uninitialized\n" );
   }
   else
      JS_ReportError( cx, "Usage: file.write( string );" );

   return JS_TRUE ;
}

static JSFunctionSpec fileMethods_[] = {
    {"isOpen",    jsFileIsOpen,   0 },
    {"close",     jsFileClose,    0 },
    {"onCharIn",  jsFileOnCharIn, 0 },
    {"onLineIn",  jsFileOnLineIn, 0 },
    {"read",      jsFileRead,     0 },
    {"write",     jsFileWrite,    0 },
    {0}
};

//
// constructor for the screen object
//
static JSBool jsFile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
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
      JSString *jsMode = JSVAL_TO_STRING( argv[1] );
      int const mode = modeFromString( JS_GetStringBytes( jsMode ) );
      int const fd = open( fileName, mode );
      if( 0 <= fd )
      {
         obj = JS_NewObject( cx, &jsFileClass_, NULL, NULL );
   
         if( obj )
         {
            filePollHandler_t *handler = new filePollHandler_t( fd, pollHandlers_, cx );
            JS_SetPrivate( cx, obj, handler );
            JS_DefineProperty( cx, obj, "path", argv[0], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
            JS_DefineProperty( cx, obj, "mode", argv[1], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
            *rval = OBJECT_TO_JSVAL(obj);
         }
         else
         {
            JS_ReportError( cx, "Allocating file %s\n", fileName );   
            close( fd );
         }
      }
      else
         JS_ReportError( cx, "%s opening file %s\n", strerror( errno ), fileName );
   }
   else
      JS_ReportError( cx, "Usage: var f = new file( 'path', 'r|w|rw' );" );

   return JS_TRUE;
}


bool initJSFileIO( JSContext *cx, JSObject *glob )
{
   fileProto = JS_InitClass( cx, glob, NULL, &jsFileClass_,
                            jsFile, 1,
                            fileProperties_, 
                            fileMethods_,
                            0, 0 );
   if( fileProto )
   {
      JS_AddRoot( cx, &fileProto );
      return JS_DefineFunctions( cx, glob, _functions);
   }
   else
      JS_ReportError( cx, "Initializing file class\n" );
   
   return false ;
}
