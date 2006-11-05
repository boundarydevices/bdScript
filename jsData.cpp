/*
 * Module jsData.cpp
 *
 * This module defines the methods of the jsData_t class as 
 * declared in jsData.h
 *
 * Change History : 
 *
 * $Log: jsData.cpp,v $
 * Revision 1.2  2006-11-05 18:19:21  ericn
 * -allow nested jsData_t's
 *
 * Revision 1.1  2006/10/16 22:45:41  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "jsData.h"
#include <string.h>

static JSBool
data_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
               JSObject **objp)
{
    if ((flags & JSRESOLVE_ASSIGNING) == 0) {
        JSBool resolved;

        if (!JS_ResolveStandardClass(cx, obj, id, &resolved))
            return JS_FALSE;
        if (resolved) {
            *objp = obj;
            return JS_TRUE;
        }
    }

    return JS_TRUE;
}

static JSClass data_class = {
    "data",                      JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,             JS_PropertyStub,
    JS_PropertyStub,             JS_PropertyStub,
    JS_EnumerateStandardClasses, (JSResolveOp)data_resolve,
    JS_ConvertStub,              JS_FinalizeStub
};

static void stdError( JSContext *cx, const char *message, JSErrorReport *report)
{
   JSObject *obj ;
   void     *priv ;
   if( ( 0 != ( obj = JS_GetGlobalObject(cx) ) )
       &&
       ( 0 != ( priv = JS_GetInstancePrivate( cx, obj, &data_class, 0 ) ) ) )
   {
      ( (jsData_t *)priv )->setErrorMsg( message, report->filename, report->lineno );
      return ;
   }

   fprintf( stderr, "Error %s\n", message );
   fprintf( stderr, "file %s, line %u\n", report->filename, report->lineno );
}

jsData_t::jsData_t( char const *scriptlet, 
                    unsigned length,
                    char const *fileName )
   : fileName_( fileName ? fileName : "<input>" )
   , ownIt_( true )
   , rt_( JS_NewRuntime(4L * 1024L * 1024L) )
   , cx_( rt_ ? JS_NewContext(rt_, 8192): 0 )
   , obj_( cx_ ? JS_NewObject(cx_, &data_class, NULL, NULL): 0 )
{
   errorMsg_[0] = 0 ;
   if( initialized() ){
      JS_InitStandardClasses(cx_, obj_);
      JS_SetErrorReporter( cx_, stdError );
      JS_SetPrivate( cx_, obj_, this );
      jsval rv ;
      evaluate( scriptlet, length, rv );
      if( '\0' != errorMsg_[0] ){
         obj_ = 0 ; // destroyed with context
         JS_DestroyContext(cx_);
         cx_ = 0 ;
         JS_DestroyRuntime(rt_);
         rt_ = 0 ;
      }
   }
}

jsData_t::jsData_t( 
   JSRuntime  *rt,
   JSContext  *cx,
   JSObject   *rootObj,
   char const *fileName )
   : fileName_( fileName ? fileName : "<input>" )
   , ownIt_( false )
   , rt_( rt )
   , cx_( cx )
   , obj_( rootObj )
{
}

jsData_t::~jsData_t( void )
{
   if( ownIt_ ){
      if( cx_ ){
         JS_DestroyContext(cx_);
      }
      if( rt_ ){
         JS_DestroyRuntime(rt_);
      }
   }
}

bool jsData_t::evaluate( 
   char const *scriptlet, 
   unsigned    length,
   jsval      &rval,
   JSObject   *obj ) const 
{
   bool worked ;
   if( 0 == obj )
      obj = obj_ ;
   worked = JS_EvaluateScript( cx_, obj, scriptlet, length, fileName_, 1, &rval );
   return worked ;
}

void jsData_t::setErrorMsg( char const *msg, char const *fileName, unsigned line )
{
   snprintf( errorMsg_, sizeof(errorMsg_), "Line %u:%s", line, msg );
}

bool jsData_t::getInt( 
   char const *name, 
   int        &value,
   JSObject   *obj 
)  const 
{
   jsval rv ;
   if( evaluate( name, strlen( name ), rv, obj ) 
       &&
       JSVAL_IS_INT( rv ) )
   {
      value = JSVAL_TO_INT( rv );
      return true ;
   }

   return false ;
}

bool jsData_t::getDouble( 
   char const *name, 
   double     &value,
   JSObject   *obj 
) const 
{
   jsval rv ;
   if( evaluate( name, strlen( name ), rv, obj ) 
       &&
       JS_ValueToNumber( cx_, rv, &value ) )
   {
      return true ;
   }

   return false ;
}

bool jsData_t::getBool( 
   char const *name, 
   bool &value,
   JSObject *obj 
) const 
{
   jsval rv ;
   if( evaluate( name, strlen( name ), rv, obj ) 
       &&
       JSVAL_IS_BOOLEAN( rv ) )
   {
      value = ( 0 != JSVAL_TO_BOOLEAN( rv ) );
      return true ;
   }

   return false ;
}

bool jsData_t::getString( 
   char const *name, 
   char *value, 
   unsigned maxLen, 
   unsigned &length,
   JSObject *obj 
) const 
{
   jsval rv ;
   JSString *s ;
   if( evaluate( name, strlen( name ), rv, obj ) 
       &&
       JSVAL_IS_STRING( rv ) 
       &&
       ( 0 != ( s = JSVAL_TO_STRING( rv ) ) ) )
   {
      unsigned sLen = JS_GetStringLength( s );
      if( sLen >= maxLen )
         sLen = maxLen - 1 ;
      strncpy( value, JS_GetStringBytes(s), sLen );
      value[sLen] = '\0' ;
      length = sLen ;
      return true ;
   }

   return false ;
   return false ;
}

#ifdef MODULETEST
#include "memFile.h"
#include <string.h>

int main( int argc, char const * const argv[] )
{
   if( 1 < argc ){
      for( int arg = 1 ; arg < argc ; arg++ ){
         char const *const fileName = argv[arg];
         memFile_t fIn( fileName );
         if( fIn.worked() ){
            jsData_t data( (char const *)fIn.getData(), fIn.getLength(), argv[arg] );
            if( data.initialized() ){
               printf( "%s\n", fileName );
               char inBuf[512];
               while( fgets( inBuf, sizeof(inBuf), stdin ) ){
                  jsval rv ;
                  if( data.evaluate( inBuf, strlen( inBuf ), rv ) ){
                     JSType type = JS_TypeOfValue(data.cx(), rv);
                     printf( "evaluated: %d\n", type );
                     JSString *str = JS_ValueToString(data.cx(), rv);
                     if( str ){
                        fwrite( JS_GetStringBytes( str ), JS_GetStringLength( str ), 1, stdout );
                        printf( "\n" );
                        if( JSTYPE_OBJECT == type ){
                           JSIdArray *elements = JS_Enumerate(data.cx(),JSVAL_TO_OBJECT(rv));
                           if( elements ){
                              printf( "   %u elements\n", elements->length );
                              for( unsigned i = 0 ; i < elements->length ; i++ ){
                                 printf( "[%u] == ", i );
                                 jsval member ;
                                 if( JS_IdToValue(data.cx(), elements->vector[i], &member ) ){
                                    str = JS_ValueToString(data.cx(), member);
                                    if( str ){
                                       fwrite( JS_GetStringBytes( str ), JS_GetStringLength( str ), 1, stdout );
                                    }
                                    else
                                       printf( "NULL" );
                                 }
                                 else
                                    printf( "???" );
                                 printf( "\n" );
                              }
                              JS_DestroyIdArray(data.cx(),elements);
                           }
                        }
                     }
                     else {
                        printf( "<void>\n" );
                     }
                  }
                  else
                     printf( "error:%s\n", data.errorMsg() );
               }
            }
            else
               printf( "error: %s\n", data.errorMsg() );
         }
         else
            perror( argv[1] );
      }
   }
   return 0 ;
}

#endif
