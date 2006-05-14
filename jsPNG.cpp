/*
 * Module jsPNG.cpp
 *
 * This module defines the initialization routine
 * for jsPNG as declared and described in jsPNG.h
 *
 *
 * Change History : 
 *
 * $Log: jsPNG.cpp,v $
 * Revision 1.1  2006-05-14 14:51:36  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "jsPNG.h"

#include <unistd.h>
#include "js/jscntxt.h"
#include <assert.h>
#include <string.h>
#include "imgToPNG.h"

static JSBool
jsImgToPNG( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 1 == argc ) && JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *const rhObj = JSVAL_TO_OBJECT( argv[0] );
      jsval widthVal, heightVal, dataVal ;

      if( JS_GetProperty( cx, rhObj, "width", &widthVal )
          &&
          JSVAL_IS_INT( widthVal )
          &&
          JS_GetProperty( cx, rhObj, "height", &heightVal )
          &&
          JSVAL_IS_INT( heightVal )
          &&
          JS_GetProperty( cx, rhObj, "pixBuf", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal ) )
      {
         int width  = JSVAL_TO_INT( widthVal ); 
         int height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short const *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         if( JS_GetStringLength( pixStr ) == width * height * sizeof( pixMap[0] ) )
         {
		image_t img( pixMap, width, height);
		void const *pngData ;
		unsigned    pngSize ;
		if( imageToPNG( img, pngData, pngSize ) )
		{
			JSString *sPNG = JS_NewStringCopyN( cx, (char *)pngData, pngSize );
			*rval = STRING_TO_JSVAL(sPNG);
			free((void *)pngData);
		}
		else
		   JS_ReportError(cx, "Error converting to PNG\n" );
		img.disown();
         }
         else
            JS_ReportError( cx, "Invalid pixmap" );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   else
      JS_ReportError( cx, "Usage: imgToPNG( img )" );
   
   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"imgToPNG",           jsImgToPNG,          0 },
    {0}
};

bool initJSPNG( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}

