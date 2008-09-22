#ifndef __CALIBQUADRANT_H__
#define __CALIBQUADRANT_H__ "$Id: calibQuadrant.h,v 1.2 2008-09-22 18:36:49 ericn Exp $"

/*
 * calibQuadrant.h
 *
 * This header file declares the calibrateQuadrant_t class, which 
 * builds upon the calibrateCoefficients_t to provide a four-quadrant
 * calibration algorithm based on five input points as shown in 
 * the diagram below:
 *    
 *  -------------------------------------------------------------
 *  |                                                           |
 *  |   1                         top                        2  |
 *  |        \                                         /        |
 *  |               \                           /               |
 *  |                      \              /                     |
 *  |      left                    5                 right      |
 *  |                      /              \                     |
 *  |               /                           \               |
 *  |        /                                        \         |
 *  |   4                                                    3  |
 *  |                           bottom                          |
 *  -------------------------------------------------------------
 *  
 * It begins by producing a first-pass set of coefficients based
 * on triangle defined by 1:2:3. This is used to figure out which 
 * other triangle to use in the second pass translation.
 *
 * Change History : 
 *
 * $Log: calibQuadrant.h,v $
 * Revision 1.2  2008-09-22 18:36:49  ericn
 * [davinci] Add some coexistence stuff for davinci_code
 *
 * Revision 1.1  2008-08-23 22:00:26  ericn
 * [touch calibration] Use calibrateQuadrant algorithm
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "calibCoef.h"

class calibrateQuadrant_t {
public:
   calibrateQuadrant_t( calibratePoint_t const inputs[5],
                        unsigned width, unsigned height );
   ~calibrateQuadrant_t( void );

   inline bool isValid( void ) const { return isValid_ ; }

   bool translate( int i, int j, int &x, int &y );

   enum {
      global  = 0,
      qtop    = 1,
      qright  = 2,
      qbottom = 3,
      qleft   = 4,
      NUM_CALIBRATIONS = 5
   };

   // for testing purposes, you can see the output of various quadrants
   bool translate_force( unsigned const q, int i, int j, int &x, int &y );

private:
   bool                     isValid_ ;
   unsigned                 centerx_ ;
   unsigned                 centery_ ;
   calibrateCoefficients_t *coef_[NUM_CALIBRATIONS];
};

#endif

   

