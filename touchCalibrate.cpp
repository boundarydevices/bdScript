/*
 * Module touchCalibrate.cpp
 *
 * This module defines the methods of the touchCalibration_t
 * class as declared in touchCalibrate.h
 *
 *
 * Change History : 
 *
 * $Log: touchCalibrate.cpp,v $
 * Revision 1.1  2006-08-28 18:24:43  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "touchCalibrate.h"
#include "flashVar.h"
#include <stdio.h>
#include <math.h>

static touchCalibration_t *inst_ = 0 ;

touchCalibration_t &touchCalibration_t::get(void)
{
   if( 0 == inst_ )
      inst_ = new touchCalibration_t ;
   return *inst_ ;
}

void touchCalibration_t::translate
   ( point_t const &input,
     point_t       &translated ) const 
{
   if( haveData_ ){
      translated.x = (coef_[0]*input.x + coef_[1]*input.y + coef_[2])/65536 ;
      translated.y = (coef_[3]*input.x + coef_[4]*input.y + coef_[5])/65536 ;
   }
   else
      translated = input ;
}

void touchCalibration_t::setCalibration( char const *data )
{
   double a[6];
   if( 6 == sscanf( data, "%lf,%lf,%lf,%lf,%lf,%lf", a, a+1, a+2, a+3, a+4, a+5 ) )
   {
      haveData_ = true ;

      for( unsigned i = 0 ; i < 6 ; i++ )
         coef_[i] = (long)floor( 65536*a[i] );
   }
   else
      fprintf( stderr, "Invalid calibration settings\n" );
}

static char const calibrateVar[] = {
   "tsCalibrate"
};

touchCalibration_t::touchCalibration_t( void )
   : haveData_( false )
{
   char const *flashVar = readFlashVar( calibrateVar );
   if( flashVar )
   {
      setCalibration( flashVar );
   }
   else
      fprintf( stderr, "No touch screen settings, using raw input\n" );
}

touchCalibration_t::~touchCalibration_t( void )
{
}

