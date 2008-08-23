/*
 * Module calibQuadrant.cpp
 *
 * This module defines the methods of the calibrateQuadrant_t
 * class as declared in calibrateQuadrant.h
 *
 * Change History : 
 *
 * $Log: calibQuadrant.cpp,v $
 * Revision 1.1  2008-08-23 22:00:26  ericn
 * [touch calibration] Use calibrateQuadrant algorithm
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */


#include "calibQuadrant.h"
#include <string.h>
#include <stdio.h>


enum {      // point locations hard-coded for now
   TL = 0,
   TR = 1,
   BR = 2,
   BL = 3,
   CENTER = 4
};

typedef struct {
   unsigned points[3];
} triangle_t ;

static triangle_t const triangles_[calibrateQuadrant_t::NUM_CALIBRATIONS] = {
    { { 0, 1, 2 } }         //      global  = 0,
,   { { 0, 1, 4 } }         //      qtop    = 1,
,   { { 1, 2, 4 } }         //      qright  = 2,
,   { { 2, 3, 4 } }         //      qbottom = 3,
,   { { 3, 0, 4 } }         //      qleft   = 4,
};

static void trianglePoints( calibratePoint_t const inputs[5],
                            triangle_t const      &triangle,
                            calibratePoint_t       points[3] )
{
   points[0] = inputs[triangle.points[0]];
   points[1] = inputs[triangle.points[1]];
   points[2] = inputs[triangle.points[2]];
}

calibrateQuadrant_t::calibrateQuadrant_t
   ( calibratePoint_t const inputs[5],
     unsigned width, unsigned height )
   : isValid_( false )
{
   memset( coef_, 0, sizeof(coef_) );
   unsigned const left = inputs[TL].x ;
   unsigned const top  = inputs[TL].y ;
   unsigned const right = inputs[TR].x ;
   unsigned const bottom = inputs[BR].y ;
   unsigned const xcenter = centerx_ = inputs[CENTER].y ;
   unsigned const ycenter = centery_ = inputs[CENTER].y ;

   if( (left >= right) || (xcenter <= left) || (xcenter >= right) ){
      return ;
   }

   if( (top >= bottom) || (ycenter <= top) || (ycenter >= bottom) ){
      return ;
   }

   for( unsigned i = 0 ; i < calibrateQuadrant_t::NUM_CALIBRATIONS ; i++ ){
      calibratePoint_t points[3];
      trianglePoints( inputs, triangles_[i], points );
      coef_[i] = new  calibrateCoefficients_t(points,width,height);
      if( !coef_[i]->isValid() ){
         fprintf( stderr, "Triangle %u has co-linear points\n", i );
      }
   }
   isValid_ = true ;
}

calibrateQuadrant_t::~calibrateQuadrant_t( void )
{
   for( unsigned i = 0 ; i < NUM_CALIBRATIONS ; i++ )
      if( coef_[i] )
         delete coef_[i];
}


bool calibrateQuadrant_t::translate( int i, int j, unsigned &x, unsigned &y )
{
   unsigned tmpx, tmpy ;
   if( coef_[global]->translate(i, j, tmpx, tmpy) ){
      //
      // left/top is a bit-flag 
      //    2 == left
      //    1 == top
      int lt = 0 ;
      int xdelta = tmpx-centerx_ ;
      if( 0 > xdelta ){
         lt |= 2 ;
         xdelta = 0-xdelta ;
      }
      int ydelta = tmpy-centery_ ;
      if( 0 > ydelta ){
         lt |= 1 ;
         ydelta = 0-ydelta ;
      }

      unsigned q = 0 ;
      if( xdelta > ydelta ){
         // use left or right quadrant
         if( lt & 2 )
            q = qleft ;
         else
            q = qright ;
      } else {
         if( lt & 1 )
            q = qtop ;
         else
            q = qbottom ;
      } 

      if( coef_[q]->translate(i,j,x,y) ){
         return true ;
      }
      else
         fprintf( stderr, "!!! Error translating point %d.%d\n", i, j );
   }
   return false ;
}

bool calibrateQuadrant_t::translate_force( unsigned const q, int i, int j, unsigned &x, unsigned &y )
{
   if( (q < NUM_CALIBRATIONS) && isValid() && (0 != coef_[q]) )
      return coef_[q]->translate(i,j,x,y);

   return false ;
}

#ifdef MODULE_TEST_CALIBQUADRANT

#ifdef __arm__
#include "fbDev.h"

static void cross( fbDevice_t &fb, int x, int y, unsigned char r, unsigned char g, unsigned char b ){
   unsigned left = x >= 8 ? x-8 : 0 ;
   unsigned right = x < fb.getWidth()-8 ? x+8 : fb.getWidth()-1 ;
   fb.line( left, y, right, y, 1, r, g, b );
   unsigned top = y >= 8 ? y-8 : 0 ;
   unsigned bottom = y < fb.getHeight()-8 ? y+8 : fb.getHeight()-1 ;
   fb.line( x, top, x, bottom, 1, r, g, b );
}
#endif

int main( int argc, char const * const argv[] )
{
   if( 6 <= argc ){
      calibratePoint_t points[5];
      for( int arg = 0 ; arg < 5 ; arg++ ){
         if( !parseCalibratePoint( argv[arg+1], points[arg] ) ){
            printf( "Invalid point <%s>, use form X:Y.i.j\n", argv[arg+1] );
            return -1 ;
         }
      }
      calibrateQuadrant_t calibration( points, 800, 600 );
      if( calibration.isValid() ){
         printf( "coefficients determined... begin translation\n" );
         char inBuf[80];

#ifdef __arm__
         fbDevice_t &fb = getFB();
#endif

         while( fgets( inBuf, sizeof(inBuf), stdin ) )
         {
            int i, j ;
            if( 2 == sscanf( inBuf, "%d.%d", &i, &j ) ){
               unsigned x, y ;
               if( calibration.translate(i,j,x,y) ){
                  printf( "%d.%d == %u:%u\n", i, j, x, y );
#ifdef __arm__
                  cross( fb, (int)x, (int)y, 0xff, 0xff, 0xff );
                  if( calibration.translate_force(0,i,j,x,y) ){
                     cross( fb, (int)x, (int)y, 0xf8, 0xe4, 0x03 );
                  }
#endif
               }
               else
                  printf( "Error translating point %d.%d\n", i, j );
            }
            else
               fprintf( stderr, "Invalid point, use form i.j\n" );
         }
      }
      else
         fprintf( stderr, "Points are co-linear\n" );
   }
   else
      fprintf( stderr, "Usage: %s X:Y.i.j X:Y.i.j X:Y.i.j X:Y.i.j X:Y.i.j\n", argv[0] );

   return 0 ;
}

#endif
