#ifndef __CALIBCOEF_H__
#define __CALIBCOEF_H__ "$Id: calibCoef.h,v 1.1 2008-08-23 22:00:26 ericn Exp $"

/*
 * calibCoef.h
 *
 * This header file declares the calibrateCoefficients_t
 * class for use in touch screen calibration algorithms.
 * Given input readings from three on-screen points that
 * determine a triangle, it will generate a1..3, b1..3 
 * used to produce calibrated X and Y based on input A/D
 * readings i and j:
 *
 * x = a1 i  +  a2 j + a3
 * y = b1 i  +  b2 j + b3
 *
 * The coefficients are kept in fixed-point 16:16 form to
 * avoid floating point at run-time, but that should be 
 * hidden from the user of the class.
 *
 * Copyright Boundary Devices, Inc. 2008
 */

typedef struct {
   unsigned x, y ;      // screen location
   int i, j ;           // generally the median A/D measurement
} calibratePoint_t ;

bool parseCalibratePoint( char const       *input,    // form is X:Y.i.j
                          calibratePoint_t &point );

class calibrateCoefficients_t {
public:
   calibrateCoefficients_t( calibratePoint_t const inputs[3],
                            unsigned width, unsigned height );
   ~calibrateCoefficients_t( void );

   inline bool isValid( void ) const { return isValid_ ; }

   inline bool translate( int i, int j, unsigned &x, unsigned &y );

   enum {
      fixed_divisor = 65536,
      fixed_shift   = 16
   };

private:
   unsigned const width_ ;
   unsigned const height_ ;
   bool           isValid_ ;
   long           coef_[6];
};


bool calibrateCoefficients_t::translate( int i, int j, unsigned &x, unsigned &y )
{
   if( isValid_ ){
      long tmp = coef_[0]*i + coef_[1]*j + coef_[2];
      if( 0 > tmp ){
         tmp = 0 ;
      }
      x = (unsigned)tmp >> fixed_shift ;
      if( x >= width_ )
         x = width_-1 ;

      tmp = coef_[3]*i + coef_[4]*j + coef_[5];
      if( 0 > tmp )
         tmp = 0 ;
      y = (unsigned)tmp >> fixed_shift ;
      if( y >= height_ )
         y = height_-1 ;
      return true  ;
   }
   else
      return false ;
}

#endif

