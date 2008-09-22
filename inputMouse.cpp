/*
 * Module inputMouse.cpp
 *
 * This module defines the methods of the inputMouse_t class
 * as declared in inputMouse.h
 *
 * Copyright Boundary Devices, Inc. 2008
 */
#include "inputMouse.h"

#include <stdio.h>
#include "tickMs.h"
#include <stdlib.h>

inputMouse_t::inputMouse_t
   ( pollHandlerSet_t &set,
     char const       *devName )
   : inputPoll_t(set,devName)
   , player_( 0 )
   , x_( 0 )
   , y_( 0 )
   , isDown_( false )
   , exitMS_( 0 )
   , lastTouch_( tickMs() )
{
   char const *envExitMs = getenv("TSEXITMS");
   if( envExitMs ){
      printf( "Exit after %sms of mouse click\n", envExitMs );
      exitMS_ = strtoul(envExitMs,0,0);
      printf( "%ums\n", exitMS_ );
   }
}

void inputMouse_t::onTouch()
{
   printf( "touch %ux%u\n", x_, y_ );
   if( player_ )
      player_->FlashMousePressEvent(1, x_, y_ );
}

void inputMouse_t::onRelease( void )
{
   printf( "touch %ux%u\n", x_, y_ );
   if( player_ )
      player_->FlashMouseReleaseEvent(1, x_, y_ );
}

#define LMOUSEBUTTON 0x110

void inputMouse_t::onData( struct input_event const &event )
{
   if( !player_ ){
      printf( "no player\n" );
      return ;
   }
   if( EV_REL == event.type ){
      FlashRect r ;
      player_->FrameBufferRect(r);
      if( REL_X == event.code ){
         x_ += event.value ;
         if( r.m_xmin > x_ )
            x_ = r.m_xmin ;
         else if( r.m_xmax < x_ ){
            x_ = r.m_xmax ;
         }
      } else if( REL_Y == event.code ){
         y_ += event.value ;
         if( r.m_ymin > y_ )
            y_ = r.m_ymin ;
         else if( r.m_ymax < y_ ){
            y_ = r.m_ymax ;
         }
      } else
         printf( "rel: %x %d\n", event.code, event.value );
   } else if( EV_SYN == event.type ){
      if( player_ ){
         printf( "move to %u:%u\n", x_, y_ );
         player_->FlashMouseMoveEvent(x_,y_);
      }
   } else if( EV_KEY == event.type ){
         int down = event.value ;
         int left = ( event.code == BTN_LEFT );
         if( left ){
            isDown_ = down ;
            if( isDown_ )
               onTouch();
            else
               onRelease();
         } // left mouse button
         else if( (event.code == BTN_MIDDLE) && !down ){
            fprintf( stderr, "release middle key, exiting\n" );
            exit(1);
         } // ignore other buttons
   } else {
      printf( "type %d, code 0x%x, value %d\n",
            event.type, 
            event.code,
            event.value );
   }
}

