/*
 * Module jsMouse.cpp
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#include "jsMouse.h"
#include "js/jscntxt.h"
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#define irqreturn_t int

#include "debugPrint.h"
#include <vector>
#include "zOrder.h"
#include "jsTouch.h"
#include <assert.h>
#include "inputDevs.h"
#include "jsGlobals.h"

#if defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)
   #include "sm501Cursor.h"
   #include <linux/sm501-int.h>
   extern sm501Cursor_t *cursor_;
#elif defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
   #include "davCursor.h"
   extern davCursor_t *cursor_;
#elif defined(KERNEL_FB_PXA_HARDWARE_CURSOR) && (KERNEL_FB_PXA_HARDWARE_CURSOR == 1)
   #include "pxaCursor.h"
   extern pxaCursor_t *cursor_;
#else
#error Makefile should not build this
#endif

jsMouse_t::jsMouse_t( char const *devName )
   : inputPoll_t( pollHandlers_, devName )
   , curBox_( 0 )
   , down_( false )
{
   if( !isOpen() ){
      perror( devName );
      return ;
   }
   printf( "%s\n", __PRETTY_FUNCTION__ );
   assert( cursor_ );
   
   fbDevice_t &fb = getFB();
   cursor_->setPos(fb.getWidth()/2, fb.getWidth()/2);
   cursor_->activate();
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
#if defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1)
         if( down ){
            cursor_->setWidth(SMALL);
            cursor_->setHeight(SMALL);
         }
         else {
            cursor_->setWidth(BIG);
            cursor_->setHeight(BIG);
         }
#endif

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

jsMouse_t *mouse_ = 0;

static JSBool
jsEnableMouse( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( (0==mouse_) && (0 != cursor_) ){
      inputDevs_t inputDevs ;
      for( unsigned i = 0 ; i < inputDevs.count(); i++ ){
         unsigned evIdx ;
         inputDevs_t::type_e type ;
         inputDevs.getInfo(i,type,evIdx);
   
         if(inputDevs_t::MOUSE == type){
            char devName[512];
            snprintf( devName, sizeof(devName), "/dev/input/event%u", evIdx );
            mouse_ = new jsMouse_t(devName);
            printf( "%s:created mouse\n", __PRETTY_FUNCTION__ );
            *rval = JSVAL_TRUE ;
            break;
         }
      }
   }
   else
      JS_ReportError( cx, "Error: mouse already defined or cursor is undefined\n" );
   return JS_TRUE ;
}

static JSBool
jsDisableMouse( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
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
                                if( 0 != mouse_ ){
                                   delete mouse_ ;
                                   mouse_ = 0 ;
                                   printf( "%s:deleted mouse\n", __PRETTY_FUNCTION__ );
                                }

				*rval = JSVAL_TRUE ;
#endif
	
	return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"enableMouse",		jsEnableMouse, 		0},
    {"disableMouse",		jsDisableMouse,		0},
    {0}
};

bool initJSMouse( JSContext *cx, JSObject *glob )
{
	return JS_DefineFunctions( cx, glob, _functions);
}

