/*
 * Module jsText.cpp
 *
 * This module defines the initialization routine and 
 * JavaScript interpreter support routines for rendering
 * text to the frame buffer device as described in 
 * jsText.h
 *
 * Change History : 
 *
 * $Log: jsText.cpp,v $
 * Revision 1.19  2004-05-05 03:17:42  ericn
 * -removed unused render()
 *
 * Revision 1.18  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.17  2003/02/10 01:16:49  ericn
 * -modified to allow truncation of text
 *
 * Revision 1.16  2003/02/09 02:58:52  ericn
 * -moved font dump to ftObjs
 *
 * Revision 1.15  2003/02/07 03:01:33  ericn
 * -made freeTypeLibrary_t internal and persistent
 *
 * Revision 1.14  2003/01/31 13:29:07  ericn
 * -added getLineGap(), getHeight(), getBaseline()
 *
 * Revision 1.13  2003/01/03 16:57:00  ericn
 * -removed jsText function
 *
 * Revision 1.13  2003/01/03 16:56:41  ericn
 * -removed jsText function
 *
 * Revision 1.12  2002/12/15 20:01:09  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.11  2002/12/03 13:36:13  ericn
 * -collapsed code and objects for curl transfers
 *
 * Revision 1.10  2002/11/30 16:26:50  ericn
 * -better error checking, new curl interface
 *
 * Revision 1.9  2002/11/30 05:30:16  ericn
 * -modified to expect call from default curl hander to app-specific
 *
 * Revision 1.8  2002/11/30 00:31:29  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.7  2002/11/05 05:40:55  ericn
 * -pre-declare font::isLoaded()
 *
 * Revision 1.6  2002/11/03 17:55:51  ericn
 * -modified to support synchronous gets and posts
 *
 * Revision 1.5  2002/11/02 18:57:03  ericn
 * -removed debug stuff
 *
 * Revision 1.4  2002/11/02 18:38:02  ericn
 * -added font.render()
 *
 * Revision 1.3  2002/11/02 04:11:43  ericn
 * -added font class
 *
 * Revision 1.2  2002/10/20 16:31:06  ericn
 * -added bg color support
 *
 * Revision 1.1  2002/10/18 01:18:25  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsText.h"
#include "ftObjs.h"
#include <stdio.h>
#include "fbDev.h"
#include <ctype.h>
#include "jsCurl.h"
#include "js/jscntxt.h"
#include "jsGlobals.h"
#include "jsCurl.h"
#include "jsAlphaMap.h"

/*
static FT_Library library ;

static void initFreeType( void )
{
   static bool _init = false ;
   if( !_init )
   {
      int error = FT_Init_FreeType( &library );
      if( 0 == error )
      {
         _init = true ;
//         printf( "Initialized FreeType!\n" );
      }
      else
      {
         fprintf( stderr, "Error %d initializing freeType library\n", error );
         exit( 3 );
      }
   }
}
*/

static JSBool
jsFontDump( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   jsval     dataVal ;
   JSString *fontString ; 

   if( JS_GetProperty( cx, obj, "data", &dataVal )
       &&
       JSVAL_IS_STRING( dataVal )
       &&
       ( 0 != ( fontString = JSVAL_TO_STRING( dataVal ) ) ) )
   {
      freeTypeFont_t    font( JS_GetStringBytes( fontString ),
                              JS_GetStringLength( fontString ) );
      if( font.worked() )
      {
         font.dump();
      }
      else
         JS_ReportError( cx, "parsing font\n" );
   }
   else
      JS_ReportError( cx, "reading font data property" );
      
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static bool addRange( JSContext *cx, 
                      JSObject  *arrObj, 
                      FT_ULong  rangeStart,
                      FT_ULong  rangeEnd )
{
   jsuint numRanges ;
   if( JS_GetArrayLength(cx, arrObj, &numRanges ) )
   {
      jsval range[2];
      range[0] = INT_TO_JSVAL( rangeStart );
      range[1] = INT_TO_JSVAL( rangeEnd );
   
      if( JS_SetArrayLength( cx, arrObj, numRanges+1 ) )
      {
         JSObject *rangeObj = JS_NewArrayObject( cx, 2, range );
         if( rangeObj )
         {
            if( JS_DefineElement( cx, arrObj, numRanges, 
                                  OBJECT_TO_JSVAL( rangeObj ),
                                  0, 0, 
                                  JSPROP_ENUMERATE
                                 |JSPROP_PERMANENT
                                 |JSPROP_READONLY ) )
            {
               return true ;
            }
            else
               JS_ReportError( cx, "Error adding array element" );
         }
         else
            JS_ReportError( cx, "Error allocating new range" );
      }
      else
         JS_ReportError( cx, "Error changing array length" );
   }

   return false ;

}

#define BADCHARCODE 0xFFFFaaaa

static JSBool
jsFontCharCodes( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   jsval     dataVal ;
   JSString *fontString ; 

   if( JS_GetProperty( cx, obj, "data", &dataVal )
       &&
       JSVAL_IS_STRING( dataVal )
       &&
       ( 0 != ( fontString = JSVAL_TO_STRING( dataVal ) ) ) )
   {
      freeTypeFont_t font( JS_GetStringBytes( fontString ),
                           JS_GetStringLength( fontString ) );
      if( font.worked() )
      {
         JSObject *arrObj = JS_NewArrayObject( cx, 0, 0 );
         if( arrObj )
         {
            *rval = OBJECT_TO_JSVAL( arrObj );

            FT_ULong  startCharCode = BADCHARCODE ;
            FT_ULong  prevCharCode = BADCHARCODE ;
   
            FT_UInt   gindex;                                                
            FT_ULong  charcode = FT_Get_First_Char( font.face_, &gindex );
            while( gindex != 0 )                                            
            {                                                                
               if( charcode != prevCharCode + 1 )
               {
                  if( BADCHARCODE != prevCharCode )
                  {
                     if( !addRange( cx, arrObj, startCharCode, prevCharCode ) )
                        break;
                  }
                  startCharCode = prevCharCode = charcode ;
               }
               else
                  prevCharCode = charcode ;
               
               charcode = FT_Get_Next_Char( font.face_, charcode, &gindex );        
            }                                                                
            
            if( BADCHARCODE != prevCharCode )
               addRange( cx, arrObj, startCharCode, prevCharCode );
         }
         else
            JS_ReportError( cx, "allocating array" );
      }
      else
         JS_ReportError( cx, "parsing font\n" );
   }
   else
      JS_ReportError( cx, "reading font data property" );
   
   return JS_TRUE ;
}

static JSBool
jsFontGetBaseline( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int pointSize ;
   if( ( 1 == argc )
       &&
       ( 0 < ( pointSize = JSVAL_TO_INT( argv[0] ) ) ) )
   {
      jsval     dataVal ;
      JSString *fontString ; 
   
      if( JS_GetProperty( cx, obj, "data", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal )
          &&
          ( 0 != ( fontString = JSVAL_TO_STRING( dataVal ) ) ) )
      {
         freeTypeFont_t font( JS_GetStringBytes( fontString ),
                              JS_GetStringLength( fontString ) );
         if( font.worked() )
         {
            if( 0 != font.face_->units_per_EM )
               *rval = INT_TO_JSVAL( (pointSize * font.face_->ascender)/font.face_->units_per_EM );
            else
               *rval = INT_TO_JSVAL( (pointSize * font.face_->ascender) );
         }
         else
            JS_ReportError( cx, "invalid font data" );
      }
      else
         JS_ReportError( cx, "no font data" );
   }
   else
      JS_ReportError( cx, "usage: font.getBaseline( pointSize )" );

   return JS_TRUE ;
}

static JSBool
jsFontGetHeight( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int pointSize ;
   if( ( 1 == argc )
       &&
       ( 0 < ( pointSize = JSVAL_TO_INT( argv[0] ) ) ) )
   {
      jsval     dataVal ;
      JSString *fontString ; 
   
      if( JS_GetProperty( cx, obj, "data", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal )
          &&
          ( 0 != ( fontString = JSVAL_TO_STRING( dataVal ) ) ) )
      {
         freeTypeFont_t font( JS_GetStringBytes( fontString ),
                              JS_GetStringLength( fontString ) );
         if( font.worked() )
         {
            if( 0 != font.face_->units_per_EM )
               *rval = INT_TO_JSVAL( (pointSize * font.face_->height)/font.face_->units_per_EM );
            else
               *rval = INT_TO_JSVAL( (pointSize * font.face_->height) );
         }
         else
            JS_ReportError( cx, "invalid font data" );
      }
      else
         JS_ReportError( cx, "no font data" );
   }
   else
      JS_ReportError( cx, "usage: font.getHeight( pointSize )" );

   return JS_TRUE ;
}

static JSBool
jsFontGetLinegap( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int pointSize ;
   if( ( 1 == argc )
       &&
       ( 0 < ( pointSize = JSVAL_TO_INT( argv[0] ) ) ) )
   {
      jsval     dataVal ;
      JSString *fontString ; 
   
      if( JS_GetProperty( cx, obj, "data", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal )
          &&
          ( 0 != ( fontString = JSVAL_TO_STRING( dataVal ) ) ) )
      {
         freeTypeFont_t    font( JS_GetStringBytes( fontString ),
                                 JS_GetStringLength( fontString ) );
         if( font.worked() )
         {
            if( 0 != font.face_->units_per_EM )
               *rval = INT_TO_JSVAL( (pointSize * 
                                       (  font.face_->height
                                        - font.face_->ascender
                                        + font.face_->descender ) ) / font.face_->units_per_EM );
            else
               *rval = INT_TO_JSVAL( (pointSize * 
                                       (  font.face_->height
                                        - font.face_->ascender
                                        + font.face_->descender ) ) );
         }
         else
            JS_ReportError( cx, "invalid font data" );
      }
      else
         JS_ReportError( cx, "no font data" );
   }
   else
      JS_ReportError( cx, "usage: font.getLineGap( pointSize )" );

   return JS_TRUE ;
}

static JSBool
jsFontRender( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( ( 2 <= argc )
       &&
       ( 3 >= argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_STRING( argv[1] ) 
       &&
       ( ( 2 == argc ) 
         || 
         ( JSVAL_IS_INT( argv[2] ) ) ) )
   {
      jsval     dataVal ;
      JSString *fontString ; 
   
      if( JS_GetProperty( cx, obj, "data", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal )
          &&
          ( 0 != ( fontString = JSVAL_TO_STRING( dataVal ) ) ) )
      {
         freeTypeFont_t    font( JS_GetStringBytes( fontString ),
                                 JS_GetStringLength( fontString ) );
         if( font.worked() )
         {
            unsigned  pointSize = JSVAL_TO_INT( argv[0] );
            JSString *sParam = JSVAL_TO_STRING( argv[1] );
            
            char const *renderString = JS_GetStringBytes( sParam );
            unsigned const renderLen = JS_GetStringLength( sParam );
   
            unsigned maxWidth = ( 3 == argc ) ? JSVAL_TO_INT( argv[2] ) : 0xFFFFFFFF ;
            freeTypeString_t ftString( font, pointSize, renderString, renderLen, maxWidth );
            
            JSObject *returnObj = JS_NewObject( cx, &jsAlphaMapClass_, 0, 0 );
            if( returnObj )
            {
               *rval = OBJECT_TO_JSVAL( returnObj );
               JSString *sAlphaMap = JS_NewStringCopyN( cx, (char const *)ftString.getRow(0), ftString.getDataLength() );
               if( sAlphaMap )
               {
                  JS_DefineProperty( cx, returnObj, "pixBuf", STRING_TO_JSVAL( sAlphaMap ), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               }
               else
                  JS_ReportError( cx, "Error building alpha map string" );
   
               JS_DefineProperty( cx, returnObj, "width",    INT_TO_JSVAL(ftString.getWidth()), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               JS_DefineProperty( cx, returnObj, "height",   INT_TO_JSVAL(ftString.getHeight()), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               JS_DefineProperty( cx, returnObj, "baseline", INT_TO_JSVAL(ftString.getBaseline()), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               JS_DefineProperty( cx, returnObj, "yAdvance", INT_TO_JSVAL(ftString.getFontHeight()), 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
            }
            else
               JS_ReportError( cx, "allocating array" );
         }
         else
            JS_ReportError( cx, "parsing font\n" );
      }
      else
         JS_ReportError( cx, "reading font data property" );
   }
   else
      JS_ReportError( cx, "Usage: font.render( pointSize, 'string' );" );
   
   return JS_TRUE ;
}

static JSFunctionSpec fontMethods[] = {
    {"dump",         jsFontDump,              0 },
    {"charCodes",    jsFontCharCodes,         0 },
    {"getBaseline",  jsFontGetBaseline,       0 },
    {"getHeight",    jsFontGetHeight,         0 },
    {"getLinegap",   jsFontGetLinegap,        0 },
    {"render",       jsFontRender,            0 },
    {0}
};


enum jsFont_tinyId {
   FONT_ISLOADED,
   FONT_DATA
};


extern JSClass jsFontClass_ ;

JSClass jsFontClass_ = {
  "font",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec fontProperties_[] = {
  {"isLoaded",    FONT_ISLOADED,   JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"data",        FONT_DATA,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static void fontOnComplete( jsCurlRequest_t &req, void const *data, unsigned long size )
{
   std::string sError ;
   freeTypeFont_t font( data, size );
   if( font.worked() )
   {
      JSString *fontString = JS_NewStringCopyN( req.cx_, (char const *)data, size );
      if( fontString )
      {
         JS_DefineProperty( req.cx_, req.lhObj_, "data",
                            STRING_TO_JSVAL( fontString ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
      }
      else
         sError = "Error allocating font string" ;
   }
   else
      sError = "Error loading font face" ;
         
   if( 0 < sError.size() )
   {
   }
}

static JSBool font( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsFontClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
         
         if( queueCurlRequest( thisObj, argv[0], cx, fontOnComplete ) )
         {
            return JS_TRUE ;
         }
         else
         {
            JS_ReportError( cx, "Error queueing curlRequest" );
         }
      }
      else
         JS_ReportError( cx, "Error allocating curlFile" );
   }
   else
      JS_ReportError( cx, "Usage : new font( { url:\"something\" [,useCache:true] [,onLoad=\"doSomething();\"][,onLoadError=\"somethingElse();\"] } );" );
      
   return JS_TRUE ;

}

bool initJSText( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsFontClass_,
                                  font, 1,
                                  fontProperties_, 
                                  fontMethods,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   
   return false ;
}

