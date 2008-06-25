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
#include "fbDev.h"
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#define irqreturn_t int
#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
#include <linux/sm501-int.h>
#elif defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
#include "davCursor.h"
#include "inputPoll.h"
#include "jsGlobals.h"
#include "box.h"
// #define DEBUGPRINT
#include "debugPrint.h"
#include <vector>
#include "zOrder.h"
#include "jsTouch.h"
#include <assert.h>

static davCursor_t *cursor_ = 0 ;

class jsMouse_t : public inputPoll_t {
public:
   jsMouse_t( void );
   ~jsMouse_t( void );

   virtual void onData( struct input_event const &event );
   void press(void);
   void release(void);

   box_t       *curBox_ ;
   bool         down_ ;
   unsigned     x_ ;
   unsigned     y_ ;
};

jsMouse_t::jsMouse_t( void )
   : inputPoll_t( pollHandlers_, "/dev/input/event0" )
   , curBox_( 0 )
   , down_( false )
{
   if( !isOpen() ){
      perror( "/dev/input/event0" );
      return ;
   }
   printf( "%s\n", __PRETTY_FUNCTION__ );
   assert( cursor_ );
   
   fbDevice_t &fb = getFB();
   cursor_->setPos(fb.getWidth()/2_, fb.setWidth()/2);
}

jsMouse_t::~jsMouse_t( void )
{
   printf( "%s\n", __PRETTY_FUNCTION__ );
}

#define LMOUSEBUTTON 0x110
#define RMOUSEBUTTON 0x111

#define BIG   5
#define SMALL 3

void jsMouse_t::onData( struct input_event const &event )
{
   assert( cursor_ );

   cursor_->getPos(x_,y_);

   switch( event.type ){
      case EV_SYN:
         cursor_->setPos(x_,y_);
         if( down_ ){
            press();
         }
         break;
      case EV_REL: {
         fbDevice_t &fb = getFB();
         int value = (int)event.value ;
         int pos = (int)( (REL_X == event.code) ? x_ : y_ );
         int max = (int)( (REL_X == event.code) ? fb.getWidth() : fb.getHeight() ) - 1 ;
         pos += value ;
         if( 0 > pos )
            pos = 0 ;
         else if( max < pos ){
            pos = max ;
         }
         if( REL_X == event.code )
            x_ = pos ;
         else
            y_ = pos ;
         cursor_->setPos(x_,y_);
         break;
      }
      case EV_KEY: {
         int down = event.value ;
         int left = ( event.code == LMOUSEBUTTON );
         if( down ){
            cursor_->setWidth(SMALL);
            cursor_->setHeight(SMALL);
         }
         else {
            cursor_->setWidth(BIG);
            cursor_->setHeight(BIG);
         }

         if( left && ( down_ != down ) ){
            if( down ){
               press();
            }
            else {
               release();
            }
            down_ = down ;
         } // left button state changed

         break;
      }
      default:
         inputPoll_t::onData(event);
   }
}

void jsMouse_t::press( void )
{
   if( 0 != curBox_ )
   {
      if( ( x_ >= curBox_->xLeft_ )
          &&
          ( x_ <= curBox_->xRight_ )
          &&
          ( y_ >= curBox_->yTop_ )
          &&
          ( y_ <= curBox_->yBottom_ ) )
      {
         curBox_->onTouchMove_( *curBox_, x_, y_ );
         debugPrint( "move on box\n" );
         return ;
      } // still on this button
      else
      {
         debugPrint( "%s: moveOff %u:%u \n", __PRETTY_FUNCTION__, x_, y_ );
         curBox_->onTouchMoveOff_( *curBox_, x_, y_ );
         curBox_ = 0 ;
         debugPrint( "move off box\n" );
      } // moved off of the button
   } // have a box
   else
      debugPrint( "No current box\n" );

   std::vector<box_t *> boxes = getZMap().getBoxes( x_, y_ );
   if( 0 < boxes.size() )
   {
      curBox_ = boxes[0];
      curBox_->onTouch_( *boxes[0], x_, y_ );
      debugPrint( "new box %p\n", curBox_ );
   }
   else
      debugPrint( "No matching box\n" );
}

void jsMouse_t::release( void )
{
   if( 0 != curBox_ )
   {
      curBox_->onRelease_( *curBox_, curBox_->lastTouchX_, curBox_->lastTouchY_ );
      curBox_ = 0 ;
   } // touching, move or release box
   else {
      debugPrint( "No current box\n" );
      onRelease();
   }
}

static jsMouse_t *mouse_ = 0 ;

#else
#error Makefile should not build this
#endif

static JSBool
jsSetCursorImage( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
	*rval = JSVAL_FALSE ;

	JSObject *rhObj ;
	if ( ( 1 == argc )
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
				unsigned short const *pixData = (unsigned short const *)JS_GetStringBytes( sPixMap );
				dither_t dither( pixData, bmWidth, bmHeight );

				unsigned sm501_cursorWidth = 64 ;
				unsigned sm501_cursorHeight = 64 ;
				//
				// SM-501 cursor is 64x64 pixels, each of 
				// which is two bits long:
				//	00	- transparent
				//	01	- color 0 (black)
				//	02 	- color 1 (white)
				unsigned cursorBytes = sm501_cursorWidth*sm501_cursorHeight*2/8 ; 
				fbDevice_t &fb = getFB();
				unsigned long *cursorLongs = (unsigned long *)fb.getRow(fb.getHeight());
				memset( cursorLongs, 0, cursorBytes );

				// cursor is right after panel display area
				unsigned const height = bmHeight > sm501_cursorHeight ? sm501_cursorHeight : bmHeight ;
				unsigned const width =  bmWidth > sm501_cursorWidth ? sm501_cursorWidth : bmWidth ;

				for( unsigned y = 0 ; y < height ; y++ ){
					unsigned long accum = 0 ;
					unsigned long shift = 0 ;
					unsigned char whichLong = 0 ;

					for( unsigned x = 0 ; x < width ; x++ ){
						unsigned char value ;
						if( alpha && alpha[x] ){
							value = 2-dither.isBlack(x,y);
						}
						else
							value = 0 ;
						accum |= value<<shift ;
						shift += 2 ;
						if( 32 <= shift ){
							cursorLongs[whichLong++] = accum ;
							accum = 0 ;
							shift = 0 ;
						}
					}
					cursorLongs[whichLong] = accum ;
					cursorLongs += (sm501_cursorWidth/2/8);
					alpha += bmWidth ;
				}

				*rval = JSVAL_TRUE ;
#elif defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
JS_ReportError(cx, "create cursor here\n" );
                                if( 0 == cursor_ ){
                                   cursor_ = new davCursor_t();
                                   cursor_->setWidth(BIG);
                                }
				*rval = JSVAL_TRUE ;
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
#elif defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
if( cursor_ ){
   cursor_->setPos( x, y );
}
else
   JS_ReportError(cx, "set cursor location to %u, %u (no cursor)\n", x, y );
				*rval = JSVAL_TRUE ;
#else
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
#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
	fbDevice_t &fb = getFB();
	unsigned long reg = SMIPCURSOR_ADDR ;
	int res = ioctl( fb.getFd(), SM501_READREG, &reg );
	if( 0 == res ){
		struct reg_and_value rv ;
		rv.reg_ = SMIPCURSOR_ADDR ;
		rv.value_ = reg | 0x80000000 ;
		res = ioctl( fb.getFd(), SM501_WRITEREG, &rv );
		if( 0 == res ){
			*rval = true ;
		}
		else
                        JS_ReportError( cx, "saving SM-501 CURSORADDR\n" );

	}
	else
		JS_ReportError( cx, "reading SM-501 CURSORADDR\n" );
	
#elif defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
                                if( 0 == cursor_ )
                                   cursor_ = new davCursor_t();
                                if( 0 == mouse_ ){
                                   mouse_ = new jsMouse_t();
                                   printf( "%s:created mouse\n", __PRETTY_FUNCTION__ );
                                }
				*rval = JSVAL_TRUE ;
#else
#endif
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
#elif defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
                                if( 0 != cursor_ ){
                                   delete cursor_ ;
                                   cursor_ = 0 ;
                                }
                                if( 0 != mouse_ ){
                                   delete mouse_ ;
                                   mouse_ = 0 ;
                                   printf( "%s:deleted mouse\n", __PRETTY_FUNCTION__ );
                                }

				*rval = JSVAL_TRUE ;
#else
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

static JSFunctionSpec _functions[] = {
    {"setCursorImage",		jsSetCursorImage,	0 },
    {"setCursorLocation",	jsSetCursorLocation,	0 },
    {"enableCursor",		jsEnableCursor, 	0 },
    {"disableCursor",		jsDisableCursor,	0 },
    {"trackCursor",		jsTrackCursor,		0 },
    {"dontTrackCursor",		jsDontTrackCursor,	0 },
    {0}
};

bool initJSCursor( JSContext *cx, JSObject *glob )
{
	return JS_DefineFunctions( cx, glob, _functions);
}

