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
 * Revision 1.4  2008-08-28 21:14:39  ericn
 * [touchCalibrate] Fix wire count declaration
 *
 * Revision 1.3  2008-08-23 22:00:26  ericn
 * [touch calibration] Use calibrateQuadrant algorithm
 *
 * Revision 1.2  2006-08-29 01:07:59  ericn
 * -clamp to screen bounds
 *
 * Revision 1.1  2006/08/28 18:24:43  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "touchCalibrate.h"
#include "flashVar.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

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
   if( data_ ){
      data_->translate(input.x,input.y,translated.x, translated.y);
   }
   else
      translated = input ;
}

void touchCalibration_t::setCalibration( char const *data )
{
   if( data_ ){
      delete data_ ;
      data_ = 0 ;
   }
   fbDevice_t &fb = getFB();
   char temp[512];
   strncpy( temp, data, sizeof(temp)-1 );
   temp[sizeof(temp)-1] = '\0' ;

   calibratePoint_t points[5];
   char *next = strtok( temp, " " );
   for( unsigned i = 0 ; i < 5 ; i++ ){
      if( next ){
         if( parseCalibratePoint( next, points[i] ) ){
            next = strtok( 0, " " );
            continue;
         }
      }
      // continue from middle
      return ;
   }

   calibrateQuadrant_t *calib = new calibrateQuadrant_t( points, fb.getWidth(), fb.getHeight() );
   if( calib->isValid() ){
      data_ = calib ;
   }
   else
      delete calib ;
}

static char const calibrateVar[] = {
   "tsCalibrate"
};

static char const tsTypeFile[] = {
   "/proc/tstype"
};

touchCalibration_t::touchCalibration_t( void )
   : data_( 0 )
{
   char const *flashVar = readFlashVar( calibrateVar );
   if( flashVar )
   {
      setCalibration( flashVar );
   }
   else {
      unsigned wires = 0 ;
      FILE *fIn = fopen( tsTypeFile, "rt" );
      if( fIn ){
         char inbuf[80];
         if( 0 != fgets( inbuf,sizeof(inbuf),fIn ) ){
            wires = inbuf[0]-'0' ;
            if( (4 == wires) || (5 == wires) ){
            }
            else {
               fprintf( stderr, "Unknown touch type <%s>\n", inbuf );
               wires = 0 ;
            }
         }
         fclose( fIn );
      }
      else {
         fprintf( stderr, "%s: No touch screen wire count, default to 4\n", tsTypeFile );
         wires = 4 ;
      }
      if( wires ){
               char inbuf[80];
               fbDevice_t &fb = getFB();
               snprintf(inbuf,sizeof(inbuf), "t%ux%u-%uwa", fb.getWidth(), fb.getHeight(), wires );
               flashVar = readFlashVar(inbuf);
               if( flashVar ){
                  setCalibration( flashVar );
               }
               else
                  fprintf( stderr, "%s: no touch screen settings\n", inbuf );
      }
   }
}

touchCalibration_t::~touchCalibration_t( void )
{
}

