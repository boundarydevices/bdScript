/*
 * Module jsTransitions.cpp
 *
 * This module defines the Javascript image transition 
 * classes as described in jsTransition.h
 *
 *
 * Change History : 
 *
 * $Log: jsTransitions.cpp,v $
 * Revision 1.1  2003-03-12 02:57:42  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsTransitions.h"
#include "fbDev.h"
#include "hexDump.h"
#include "js/jscntxt.h"
#include "jsGlobals.h"
#include "jsImage.h"

static unsigned long now_ms()
{
   struct timeval now ; gettimeofday( &now, 0 );
   return (now.tv_sec*1000)+(now.tv_usec/1000) ;
}

JSBool
jsFadeTick( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( 0 == argc )
   {
      jsval stVal ;
      jsval durVal ;
      jsval lhVal ;
      jsval rhVal ;
      jsval xVal, yVal ;
      if( JS_GetProperty( cx, obj, "startTick", &stVal ) 
          &&
          JS_GetProperty( cx, obj, "duration", &durVal ) 
          &&
          JS_GetProperty( cx, obj, "src", &lhVal )
          &&
          JS_GetProperty( cx, obj, "dest", &rhVal )
          &&
          JS_GetProperty( cx, obj, "x", &xVal )
          &&
          JS_GetProperty( cx, obj, "y", &yVal ) )
      {
         unsigned long const start = JSVAL_TO_INT( stVal );
         unsigned long const duration = JSVAL_TO_INT( durVal );
         unsigned long const msNow = now_ms();
         unsigned long const end = start+duration ;
         if( msNow < end )
         {
            unsigned long const _256ths = ((msNow-start)*256)/duration ;
//printf( "0x%02x /256ths\n", _256ths ); fflush( stdout );
      
            JSObject *const src = JSVAL_TO_OBJECT( lhVal );
            JSObject *const dest = JSVAL_TO_OBJECT( rhVal );
            jsval widthVal, heightVal, srcPixVal, destPixVal ;
            if( JS_GetProperty( cx, src, "width", &widthVal )
                &&
                JS_GetProperty( cx, src, "height", &heightVal )
                &&
                JS_GetProperty( cx, src, "pixBuf", &srcPixVal )
                &&
                JSVAL_IS_STRING( srcPixVal ) 
                &&
                JS_GetProperty( cx, dest, "pixBuf", &destPixVal )
                &&
                JSVAL_IS_STRING( destPixVal ) 
            )
            {
               int width  = JSVAL_TO_INT( widthVal ); 
               int height = JSVAL_TO_INT( heightVal );
               JSString *srcPixStr = JSVAL_TO_STRING( srcPixVal );
               unsigned short const *const srcPixMap = (unsigned short *)JS_GetStringBytes( srcPixStr );
               unsigned const srcLen = JS_GetStringLength( srcPixStr );

               JSString *destPixStr = JSVAL_TO_STRING( destPixVal );
               unsigned short const *const destPixMap = (unsigned short *)JS_GetStringBytes( destPixStr );
               unsigned const destLen = JS_GetStringLength( destPixStr );
               if( ( srcLen == destLen ) 
                   &&
                   ( srcLen == width * height * sizeof( srcPixMap[0] ) ) )
               {
                  fbDevice_t &fb = getFB();
         
                  fb.blend( JSVAL_TO_INT( xVal ), JSVAL_TO_INT( yVal ), width, height, srcPixMap, destPixMap, _256ths );
                  *rval = JSVAL_TRUE ;
               }
               else
                  JS_ReportError( cx, "Invalid pixMap(s)\n" );
            }
            else
               JS_ReportError( cx, "Object not initialized, can't draw\n" );
         }
         else
         {
            JSObject *dest = JSVAL_TO_OBJECT( rhVal );
            jsval params[2] = { xVal, yVal };
            jsImageDraw( cx, dest, 2, params, rval );
         } // just draw end item
      }
      else
         JS_ReportError( cx, "getting timeProperties" );
   } // need msNow parameter
   else
      JS_ReportError( cx, "Usage: fade.tick()" );

   return JS_TRUE ;
}

static JSFunctionSpec fadeMethods_[] = {
    {"tick",         jsFadeTick,           3 },
    {0}
};

enum jsFade_tinyId {
   FADE_SRC,
   FADE_DEST,
   FADE_DURATION,
   FADE_X,
   FADE_Y,
   FADE_STARTTICK
};

JSClass jsFadeClass_ = {
  "fade",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec fadeProperties_[] = {
  {"src",         FADE_SRC,         JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"dest",        FADE_DEST,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"duration",    FADE_DURATION,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"x",           FADE_X,           JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"y",           FADE_Y,           JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"startTick",   FADE_STARTTICK,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static JSBool fade( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 5 == argc ) 
       && 
       JSVAL_IS_OBJECT( argv[0] )
       && 
       JSVAL_IS_OBJECT( argv[1] )
       && 
       JSVAL_IS_INT( argv[2] )
       && 
       JSVAL_IS_INT( argv[3] )
       && 
       JSVAL_IS_INT( argv[4] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsFadeClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         JSObject *const src = JSVAL_TO_OBJECT( argv[0] );
         JSObject *const dest = JSVAL_TO_OBJECT( argv[1] );
         jsval lhWidth, lhHeight,
               rhWidth, rhHeight ;
         if( JS_GetProperty( cx, src, "width", &lhWidth )
             &&
             JS_GetProperty( cx, src, "height", &lhHeight )
             &&
             JS_GetProperty( cx, dest, "width", &rhWidth )
             &&
             JS_GetProperty( cx, dest, "height", &rhHeight ) )
         {
            if( ( JSVAL_TO_INT( lhWidth ) == JSVAL_TO_INT( rhWidth ) )
                &&
                ( JSVAL_TO_INT( lhHeight ) == JSVAL_TO_INT( rhHeight ) ) )
            {
               int const duration = JSVAL_TO_INT( argv[2] );
               if( 0 < duration )
               {
                  int const x = JSVAL_TO_INT( argv[3] );
                  int const y = JSVAL_TO_INT( argv[4] );
                  fbDevice_t &fbDev = getFB();
                  if( ( 0 <= x ) && ( 0 <= y ) 
                      &&
                      ( fbDev.getWidth() > x )
                      &&
                      ( fbDev.getHeight() > y ) )
                  {
                     for( int i = 0 ; i < argc ; i++ )
                        JS_DefineProperty( cx, thisObj, 
                                           fadeProperties_[i].name, 
                                           argv[i], 0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                     unsigned long const tickNow = now_ms();
                     JS_DefineProperty( cx, thisObj, fadeProperties_[argc].name, INT_TO_JSVAL( tickNow ),
                                        0, 0,  JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY );
                  }
               }
               else
                  JS_ReportError( cx, "duration must be > 0" );
            }
            else
               JS_ReportError( cx, "fade: images must be the same size\n"
                                   "%d/%d, %d/%d\n",
                               JSVAL_TO_INT( lhWidth ), JSVAL_TO_INT( rhWidth ),
                               JSVAL_TO_INT( lhHeight ), JSVAL_TO_INT( rhHeight ) );
         }
         else
            JS_ReportError( cx, "fade: getting image sizes" );
      }
      else
         JS_ReportError( cx, "Error allocating fade" );
   }
   else
      JS_ReportError( cx, "Usage : new fade( srcImg, destImg, durationMs );" );
      
   return JS_TRUE ;

}

bool initJSTransitions( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsFadeClass_,
                                  fade, 1,
                                  fadeProperties_, 
                                  fadeMethods_,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      return false ;

}

