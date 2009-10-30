/*
 * Module inputTouch.cpp
 *
 * This module defines the methods of the inputTouchScreen_t
 * class as declared in inputTouch.h
 *
 * Copyright Boundary Devices, Inc. 2008
 */


#include "inputTouch.h"
#include <stdio.h>
#include "tickMs.h"
#include <stdlib.h>
#include "debugPrint.h"

inputTouchScreen_t::inputTouchScreen_t
   ( pollHandlerSet_t &set,
     char const       *devName )
   : inputPoll_t(set,devName)
   , medianX_( 5 )
   , medianY_( 5 )
   , raw_( true )
   , i_( 0 )
   , j_( 0 )
   , mask_( false )
   , isDown_( false )
   , wasDown_( false )
   , prevX_( 0 )
   , prevY_( 0 )
   , exitMS_( 0 )
   , lastTouch_( tickMs() )
{
   setCooked();
   char const *envExitMs = getenv("TSEXITMS");
   if( envExitMs ){
      debugPrint( "Exit after %sms of touch\n", envExitMs );
      exitMS_ = strtoul(envExitMs,0,0);
      debugPrint( "%ums\n", exitMS_ );
   }
}

void inputTouchScreen_t::onTouch( unsigned x, unsigned y )
{
   long long now = tickMs();
   if( !wasDown_ ){
//      if( player_ )
//         player_->FlashMousePressEvent(1, x, y);
      lastTouch_ = now ;
   } else {
//      if( player_ )
//         player_->FlashMouseMoveEvent(x, y);
      unsigned elapsed = now-lastTouch_ ;
      if( (0 < exitMS_) && (exitMS_ < elapsed) ){
         fprintf( stderr, "TouchExit expired, exiting\n" );
         exit(1);
      }
   } // no move callback
   prevX_ = x ;
   prevY_ = y ;
}

void inputTouchScreen_t::onRelease( void )
{
//   if( player_ )
//      player_->FlashMouseReleaseEvent(1, prevX_, prevY_);
   medianX_.reset(); medianY_.reset();
}

void inputTouchScreen_t::onData( struct input_event const &event )
{
debugPrint( "%s: type %d, code 0x%x, value %d\n", __PRETTY_FUNCTION__, event.type,  event.code, event.value );
   if( EV_ABS == event.type ){
      if( ABS_X == event.code ){
         i_ = event.value ;
         mask_ |= 1<<ABS_X ;
      } else if( ABS_Y == event.code ){
         j_ = event.value ;
         mask_ |= 1<<ABS_Y ;
      } else if( ABS_PRESSURE == event.code ){
         isDown_ = (0 != event.value);
      }
   } else if( EV_SYN == event.type ){
      if( (isDown_ != wasDown_) && (!isDown_) ){
         wasDown_ = isDown_ ;
debugPrint( "%s: released\n", __PRETTY_FUNCTION__ );
         onRelease();
         mask_ = 0 ;
      } else if( mask_ == ((1<<ABS_X)|(1<<ABS_Y)) ){
         if( !raw_ ){
            point_t raw ; raw.x = i_ ; raw.y = j_ ;
            point_t cooked ;
            touchCalibration_t::get().translate(raw,cooked);
            int const fbw = SCREENWIDTH();
            int const fbh = SCREENHEIGHT();

            i_ = cooked.x ; if( 0 > i_ ) i_ = 0 ; if( i_ >= fbw ) i_ = fbw-1 ;
            j_ = cooked.y ; if( 0 > j_ ) j_ = 0 ; if( j_ >= fbh ) j_ = fbh-1 ;
            cooked.x = i_ ;
            cooked.y = j_ ;
         }
         medianX_.feed( i_ );
         medianY_.feed( j_ );

         unsigned short x, y ;
         if( medianX_.read(x) && medianY_.read(y) ){
debugPrint( "%s: touch %d:%d\n", __PRETTY_FUNCTION__, x, y );
            onTouch(x,y);
            wasDown_ = true ;
         }
         mask_ = 0 ; i_ = j_ = 0 ;
      }
   } else {
      debugPrint( "type %d, code 0x%x, value %d\n",
            event.type, 
            event.code,
            event.value );
   }
}

void inputTouchScreen_t::setCooked(void)
{
   raw_ = !touchCalibration_t::get().haveCalibration();
   medianX_.reset();
   medianY_.reset();
   prevX_ = 0 ;
   prevY_ = 0 ;
}

void inputTouchScreen_t::setRaw(void)
{
   raw_ = true ;
   medianX_.reset();
   medianY_.reset();
   prevX_ = 0 ;
   prevY_ = 0 ;
}

