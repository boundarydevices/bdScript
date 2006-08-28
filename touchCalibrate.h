#ifndef __TOUCHCALIBRATE_H__
#define __TOUCHCALIBRATE_H__ "$Id: touchCalibrate.h,v 1.1 2006-08-28 18:24:43 ericn Exp $"

/*
 * touchCalibrate.h
 *
 * This header file declares the touchCalibration_t
 * class, which is used to translate raw A/D readings
 * to calibrated values based on the value of the
 * 'tsCalibrate' setting in a flash variable.
 *
 * It defines a static 'get()' method to return the
 * singleton object (an app will never need more than
 * one, right?) and a translate() method to perform the
 * actual translation.
 *
 * Change History : 
 *
 * $Log: touchCalibrate.h,v $
 * Revision 1.1  2006-08-28 18:24:43  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbDev.h"

class touchCalibration_t {
public:
   static touchCalibration_t &get(void);

   void translate( point_t const &input,
                   point_t       &translated ) const ;

   bool haveCalibration( void ) const { return haveData_ ; }

   void setCalibration( char const *data );

private:
   touchCalibration_t( void );
   ~touchCalibration_t( void );
   bool  haveData_ ;
   long  coef_[6];     // 24.8
};


#endif

