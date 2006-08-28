/*
 * Module odomTouch.cpp
 *
 * This module defines the methods of the odomTouch_t
 * class as declared in odomTouch.h.
 *
 *
 * Change History : 
 *
 * $Log: odomTouch.cpp,v $
 * Revision 1.1  2006-08-28 18:24:43  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomTouch.h"
#include <stdio.h>
#include "touchCalibrate.h"
#include "touchSignal.h"

static odomTouch_t *inst_ = 0 ;

odomTouch_t &odomTouch_t::get(void)
{
   if( 0 == inst_ )
      inst_ = new odomTouch_t ;
   return *inst_ ;
}

void odomTouch_t::onTouch( unsigned x, unsigned y )
{
   printf( "touch: %u:%u\n", x, y );
}

void odomTouch_t::onRelease( void )
{
   printf( "release\n" );
}


static void touchHandler( int x, int y, unsigned pressure, timeval const &tv )
{
   if( 0 < pressure ){
      touchCalibration_t const &calibration = touchCalibration_t::get();
      point_t in ; in.x = x ; in.y = y ;
      point_t out ;
      calibration.translate( in, out );
   
      odomTouch_t::get().onTouch( out.x, out.y );
   } else {
      odomTouch_t::get().onRelease();
   }
}

odomTouch_t::odomTouch_t( void )
{
   touchSignal_t::get(touchHandler);
}

odomTouch_t::~odomTouch_t( void )
{
}


