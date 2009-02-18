/*
 * Module jsCursor.cpp
 *
 * This module defines the initialization routine
 * for the Javascript cursor bindings as described
 * in jsCursor.h
 *
 *
 * Change History : 
 *
 * $Log: jsCursor.cpp,v $
 * Revision 1.8  2009-02-18 18:30:55  valli
 * fixed broken davinci build because of pxacursor changes
 *
 * Revision 1.7  2009-02-16 23:08:27  valli
 * added support for pxaCursor to jsCursor.
 * moved mouse support code to a separate source file, separated code to
 * activate mouse from the jsEnableCursor code.
 * jsEnableCursor code would activate only the cursor.
 *
 * Revision 1.6  2008-10-29 15:30:36  ericn
 * [sm501 cursor] Added full mouse cursor support on SM-501
 *
 * Revision 1.5  2008-09-22 17:25:54  ericn
 * [cursor] Conditionally enable cursor (if mouse attached)
 *
 * Revision 1.4  2008-07-17 20:49:50  ericn
 * -[jsCursor] Fix typos
 *
 * Revision 1.3  2008-06-25 01:19:38  ericn
 * add real mouse support (Davinci only)
 *
 * Revision 1.2  2008-06-24 23:32:27  ericn
 * [jsCursor] Add support for Davinci HW cursor
 *
 * Revision 1.1  2007/07/07 00:25:52  ericn
 * -[bdScript] added mouse cursor and input handling
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#include "jsCursor.h"
#include "jsImage.h"
#include "js/jscntxt.h"
#include "dither.h"
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#define irqreturn_t int

#include "debugPrint.h"
#include <vector>
#include "zOrder.h"
#include "jsTouch.h"
#include <assert.h>
#include "jsGlobals.h"
#include "box.h"

#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
   #include <linux/sm501-int.h>
   #include "sm501Cursor.h"
   sm501Cursor_t *cursor_ = 0 ;
#elif defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
   #include "davCursor.h"
   davCursor_t *cursor_ = 0 ;
#elif defined(KERNEL_FB_PXA_HARDWARE_CURSOR) && (KERNEL_FB_PXA_HARDWARE_CURSOR == 1)
   #include "pxaCursor.h"
   pxaCursor_t *cursor_ = 0;
#else
#error Makefile should not build this
#endif

#define BIG   5
#define SMALL 3

static JSBool
jsSetCursorImage( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
	*rval = JSVAL_FALSE ;

	JSObject *rhObj ;
	if ( ( 1 <= argc )
	     &&
	     JSVAL_IS_OBJECT( argv[0] ) 
	     &&
	     ( 0 != ( rhObj = JSVAL_TO_OBJECT( argv[0] ) ) )
	     &&
	     JS_InstanceOf( cx, rhObj, &jsImageClass_, NULL ) )
	{
		jsval     vPixMap ;
		jsval     vWidth ;
		jsval     vHeight ;
		JSString *sPixMap ;

		if ( JS_GetProperty( cx, rhObj, "pixBuf", &vPixMap )
		     &&
		     JSVAL_IS_STRING( vPixMap )
		     &&
		     ( 0 != ( sPixMap = JSVAL_TO_STRING( vPixMap ) ) )
		     &&
		     JS_GetProperty( cx, rhObj, "width", &vWidth )
		     &&
		     JSVAL_IS_INT( vWidth )
		     &&
		     JS_GetProperty( cx, rhObj, "height", &vHeight )
		     &&
		     JSVAL_IS_INT( vHeight ) )
		{
			jsval     alphaVal ;
			JSString *sAlpha ;
			unsigned char const *alpha = 0 ;

			if ( JS_GetProperty( cx, rhObj, "alpha", &alphaVal ) 
			     &&
			     JSVAL_IS_STRING( alphaVal )
			     &&
			     ( 0 != ( sAlpha = JSVAL_TO_STRING( alphaVal ) ) ) )
			{
				alpha = (unsigned char const *)JS_GetStringBytes( sAlpha );
			} // have alpha channel... use it

			unsigned const bmWidth    = JSVAL_TO_INT( vWidth );
			unsigned       bmHeight   = JSVAL_TO_INT( vHeight );
			if ( JS_GetStringLength( sPixMap ) == bmWidth*bmHeight*2 )
			{
#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
                                if( 0 == cursor_ ){
                                   unsigned short const *pixData = (unsigned short const *)JS_GetStringBytes( sPixMap );
                                   image_t img(pixData, bmWidth, bmHeight, alpha);
                                   cursor_ = new sm501Cursor_t( img );
   
                                   *rval = JSVAL_TRUE ;
                                }
                                else
                                   JS_ReportError(cx, "Mouse already defined" );

#elif defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
JS_ReportError(cx, "create cursor here\n" );
                                if( 0 == cursor_ ){
                                   cursor_ = new davCursor_t();
                                   cursor_->setWidth(BIG);
                                }
				*rval = JSVAL_TRUE ;
#elif defined(KERNEL_FB_PXA_HARDWARE_CURSOR) && (KERNEL_FB_PXA_HARDWARE_CURSOR == 1)
				int mode = 0;
				if(argc == 2 && JSVAL_IS_INT(argv[1])) {
					mode = JSVAL_TO_INT(argv[1]);
				}
                                if(0 == cursor_) {
                                   unsigned short const *pixData = (unsigned short const *)JS_GetStringBytes( sPixMap );
                                   image_t img(pixData, bmWidth, bmHeight, alpha);
                                   cursor_ = new pxaCursor_t(mode);
                                   cursor_->setImage(img);
				   img.disown();

                                   *rval = JSVAL_TRUE;
                                }
                                else { /* update with new image */
                                   unsigned short const *pixData = (unsigned short const *)JS_GetStringBytes( sPixMap );
                                   image_t img(pixData, bmWidth, bmHeight, alpha);
                                   cursor_->setImage(img);
				   img.disown();

                                   *rval = JSVAL_TRUE;
				}
#else
#endif
			}
			else
				JS_ReportError( cx, "Invalid pixMap" );
		}
		else
			JS_ReportError( cx, "Error retrieving image fields" );

	}
	else
		JS_ReportError(cx, "Usage: setCursorImage(image)\n" );
	return JS_TRUE ;
}

static JSBool
jsSetCursorLocation( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
	*rval = JSVAL_FALSE ;
	if( ( 2 == argc )
	    &&
	    JSVAL_IS_INT( argv[0] )
	    &&
	    JSVAL_IS_INT( argv[1] ) )
	{
		fbDevice_t &fb = getFB();
		unsigned x = JSVAL_TO_INT(argv[0]);
		unsigned y = JSVAL_TO_INT(argv[1]);
		if( (x < fb.getWidth())
		    &&
		    (y < fb.getHeight()) )
		{
#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
			unsigned long reg = SMIPCURSOR_LOC ;
			int res = ioctl( fb.getFd(), SM501_READREG, &reg );
			if( 0 == res ){
				struct reg_and_value rv ;
				rv.reg_ = SMIPCURSOR_LOC ;
				rv.value_ = (y << 16) | x ;

				res = ioctl( fb.getFd(), SM501_WRITEREG, &rv );
				if( 0 == res ){
					*rval = true ;
				}
				else
					JS_ReportError( cx, "saving SM-501 CURSORADDR\n" );
		
			}
			else
				JS_ReportError( cx, "reading SM-501 CURSORADDR\n" );
#else
if( cursor_ ){
   cursor_->setPos( x, y );
}
else
   JS_ReportError(cx, "set cursor location to %u, %u (no cursor)\n", x, y );
				*rval = JSVAL_TRUE ;
#endif

		}
		else
			JS_ReportError( cx, "Cursor out of range[%u,%u]\n", fb.getWidth(), fb.getHeight() );
	}
	else
                JS_ReportError(cx, "Usage: setCursorLocation(x,y)\n", __FUNCTION__ );
	return JS_TRUE ;
}

static JSBool
jsEnableCursor( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
#if defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
            if( 0 == cursor_ )
               cursor_ = new davCursor_t();
#endif
   assert(cursor_);
   cursor_->activate();
   *rval = JSVAL_TRUE;
   return JS_TRUE ;
}

static JSBool
jsDisableCursor( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
	*rval = JSVAL_FALSE ;
#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
	fbDevice_t &fb = getFB();
	unsigned long reg = SMIPCURSOR_ADDR ;
	int res = ioctl( fb.getFd(), SM501_READREG, &reg );
	if( 0 == res ){
		struct reg_and_value rv ;
		rv.reg_ = SMIPCURSOR_ADDR ;
		rv.value_ = reg & ~0x80000000 ;
		res = ioctl( fb.getFd(), SM501_WRITEREG, &rv );
		if( 0 == res ){
			*rval = true ;
		}
		else
                        JS_ReportError( cx, "saving SM-501 CURSORADDR\n" );

	}
	else
		JS_ReportError( cx, "reading SM-501 CURSORADDR\n" );
#else
                                if( 0 != cursor_ ){
                                   delete cursor_ ;
                                   cursor_ = 0 ;
                                }

				*rval = JSVAL_TRUE ;
#endif
	
	return JS_TRUE ;
}

static JSBool
jsTrackCursor( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
	JS_ReportError(cx, "%s: Not yet implemented\n", __FUNCTION__ );
	return JS_TRUE ;
}

static JSBool
jsDontTrackCursor( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
	JS_ReportError(cx, "%s: Not yet implemented\n", __FUNCTION__ );
	return JS_TRUE ;
}

#if defined(KERNEL_FB_PXA_HARDWARE_CURSOR) && (KERNEL_FB_PXA_HARDWARE_CURSOR == 1)
static JSBool
jsSetMode( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
	if(JSVAL_IS_INT(argv[0])) {
		int mode = JSVAL_TO_INT(argv[0]);
		if(cursor_ == 0) {
			cursor_ = new pxaCursor_t();
		}
		cursor_->setMode(mode);
	}
        return JS_TRUE ;
}
#endif

static JSFunctionSpec _functions[] = {
    {"setCursorImage",		jsSetCursorImage,	0 },
    {"setCursorLocation",	jsSetCursorLocation,	0 },
    {"enableCursor",		jsEnableCursor, 	0 },
    {"disableCursor",		jsDisableCursor,	0 },
    {"trackCursor",		jsTrackCursor,		0 },
    {"dontTrackCursor",		jsDontTrackCursor,	0 },
#if defined(KERNEL_FB_PXA_HARDWARE_CURSOR) && (KERNEL_FB_PXA_HARDWARE_CURSOR == 1)
    {"setCursorMode",		jsSetMode,		0 },
#endif
    {0}
};

bool initJSCursor( JSContext *cx, JSObject *glob )
{
	return JS_DefineFunctions( cx, glob, _functions);
}

