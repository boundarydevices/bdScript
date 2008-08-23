/*
 * Module calibCoef.cpp
 *
 * This module defines the methods of the calibrateCoefficients_t
 * class as declared in calibCoef.h
 *
 * Change History : 
 *
 * $Log: calibCoef.cpp,v $
 * Revision 1.1  2008-08-23 22:00:26  ericn
 * [touch calibration] Use calibrateQuadrant algorithm
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "calibCoef.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define rc(r,c) (r*6+c)
static void SwapRows(double* w1,double* w2)
{
   double t;
   int i=6;
   while (i) {
      t = *w1; 
      *w1++ = *w2;
      *w2++ = t;	//swap
      i--;
   }
}

inline long roundLong( double d ){
   d *= calibrateCoefficients_t::fixed_divisor ;
   return (long)round(d);
}

calibrateCoefficients_t::calibrateCoefficients_t
   ( calibratePoint_t const inputs[3],
     unsigned               width,
     unsigned               height )
   : width_( width )
   , height_( height )
   , isValid_( 0 )
{
   memset( coef_,0,sizeof(coef_));
   coef_[0] = fixed_divisor ;
   coef_[4] = fixed_divisor ;
   for( unsigned i = 0 ; i < 3 ; i++ ){
      calibratePoint_t const &pt = inputs[i];
      if( pt.x >= width_ )
         return ;
      if( pt.y >= height_ )
         return ;
   }

   if( (inputs[0].x == inputs[1].x) && (inputs[1].x == inputs[2].x) )
      return ; // co-linear in X
   if( (inputs[0].y == inputs[1].y) && (inputs[1].y == inputs[2].y) )
      return ; // co-linear in Y

   int i,j;
   double w[rc(3,6)];
   double t;

   w[rc(0,0)] = inputs[0].i;
   w[rc(1,0)] = inputs[0].j;
   w[rc(2,0)] = 1.0;

   w[rc(0,1)] = inputs[1].i;
   w[rc(1,1)] = inputs[1].j;
   w[rc(2,1)] = 1.0;

   w[rc(0,2)] = inputs[2].i;
   w[rc(1,2)] = inputs[2].j;
   w[rc(2,2)] = 1.0;

   //find Inverse of touched
   for(j=0; j<3; j++) {
      for(i=3; i<6; i++) {
	 w[rc(j,i)] = ((j+3)==i) ? 1.0 : 0;  //identity matrix tacked on end
      }
   }

   //	PrintMatrix(w);
   if(w[rc(0,0)] < w[rc(1,0)]) SwapRows(&w[rc(0,0)],&w[rc(1,0)]);
   if(w[rc(0,0)] < w[rc(2,0)]) SwapRows(&w[rc(0,0)],&w[rc(2,0)]);
   t = w[rc(0,0)];
   if(t==0) {printf("error w[0][0]=0\n"); return ; }
   w[rc(0,0)] = 1.0;
   for(i=1; i<6; i++) w[rc(0,i)] = w[rc(0,i)]/t;

   for(j=1; j<3; j++) {
      t = w[rc(j,0)];
      w[rc(j,0)] = 0;
      for(i=1; i<6; i++) w[rc(j,i)] -= w[rc(0,i)] * t;
   }

   //	PrintMatrix(w);
   if(w[rc(1,1)] < w[rc(2,1)]) SwapRows(&w[rc(1,0)],&w[rc(2,0)]);

   t = w[rc(1,1)];
   if(t==0) {printf("error w[1][1]=0\n"); return ;}
   w[rc(1,1)] = 1.0;
   for(i=2; i<6; i++) w[rc(1,i)] = w[rc(1,i)]/t;

   t = w[rc(2,1)];
   w[rc(2,1)] = 0;
   for(i=2; i<6; i++) w[rc(2,i)] -= w[rc(1,i)] * t;

   //	PrintMatrix(w);
   t = w[rc(2,2)];
   if(t==0) {printf("error w[2][2]=0\n"); return ;}
   w[rc(2,2)] = 1.0;
   for(i=3; i<6; i++) w[rc(2,i)] = w[rc(2,i)]/t;

   for(j=0; j<2; j++) {
      t = w[rc(j,2)];
      w[rc(j,2)] = 0;
      for(i=3; i<6; i++) w[rc(j,i)] -= w[rc(2,i)] * t;
   }

   //	PrintMatrix(w);
   t = w[rc(0,1)];
   w[rc(0,1)] = 0;
   for(i=3; i<6; i++) w[rc(0,i)] -= w[rc(1,i)] * t;
   //	PrintMatrix(w);

   /*
   | x1  x2  x3 |  | i1  i2  i3 | -1  =  | a1  a2  a3 |
   | y1  y2  y3 |  | j1  j2  j3 |        | b1  b2  b3 |
	  | 1   1    1 |
   */

   coef_[0] = roundLong((inputs[0].x * w[rc(0,3)]) + (inputs[1].x * w[rc(1,3)]) + (inputs[2].x * w[rc(2,3)]));
   coef_[1] = roundLong((inputs[0].x * w[rc(0,4)]) + (inputs[1].x * w[rc(1,4)]) + (inputs[2].x * w[rc(2,4)]));
   coef_[2] = roundLong((inputs[0].x * w[rc(0,5)]) + (inputs[1].x * w[rc(1,5)]) + (inputs[2].x * w[rc(2,5)]));
   coef_[3] = roundLong((inputs[0].y * w[rc(0,3)]) + (inputs[1].y * w[rc(1,3)]) + (inputs[2].y * w[rc(2,3)]));
   coef_[4] = roundLong((inputs[0].y * w[rc(0,4)]) + (inputs[1].y * w[rc(1,4)]) + (inputs[2].y * w[rc(2,4)]));
   coef_[5] = roundLong((inputs[0].y * w[rc(0,5)]) + (inputs[1].y * w[rc(1,5)]) + (inputs[2].y * w[rc(2,5)]));
   isValid_ = true ;
}

calibrateCoefficients_t::~calibrateCoefficients_t( void )
{
}

bool parseCalibratePoint( char const       *input,    // form is X:Y.i.j
                          calibratePoint_t &point )
{
   return 4 == sscanf( input, "%u:%u.%d.%d", &point.x, &point.y, &point.i, &point.j );
}


#ifdef MODULE_TEST_CALIBCOEF

int main( int argc, char const * const argv[] )
{
   if( 4 <= argc ){
      calibratePoint_t points[3];
      for( int arg = 0 ; arg < 3 ; arg++ ){
         if( !parseCalibratePoint( argv[arg+1], points[arg] ) ){
            printf( "Invalid point <%s>, use form X:Y.i.j\n", argv[arg+1] );
            return -1 ;
         }
      }
      calibrateCoefficients_t coef( points, 800, 600 );
      if( coef.isValid() ){
         printf( "coefficients determined... begin translation\n" );
         char inBuf[80];

         while( fgets( inBuf, sizeof(inBuf), stdin ) )
         {
            int i, j ;
            if( 2 == sscanf( inBuf, "%d.%d", &i, &j ) ){
               unsigned x, y ;
               if( coef.translate(i,j,x,y) ){
                  printf( "%d.%d == %u:%u\n", i, j, x, y );
               }
               else
                  printf( "Error translating point\n" );
            }
            else
               fprintf( stderr, "Invalid point, use form i.j\n" );
         }
      }
      else
         fprintf( stderr, "Points are co-linear\n" );
   }
   else
      fprintf( stderr, "Usage: %s X:Y.i.j X:Y.i.j X:Y.i.j\n", argv[0] );

   return 0 ;
}

#endif
