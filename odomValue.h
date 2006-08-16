#ifndef __ODOMVALUE_H__
#define __ODOMVALUE_H__ "$Id: odomValue.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * odomValue.h
 *
 * This header file declares the odomValue_t class,
 * to represent a dollar value comprised of a set of
 * odomDigit_t's.
 *
 * Change History : 
 *
 * $Log: odomValue.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "odomDigit.h"
#include "odomGraphics.h"
#include "fbDev.h"
#include "odomMode.h"

class odomValue_t {
public:
   odomValue_t( fbCommandList_t      &cmdList,
                odomGraphics_t const &graphics,
                unsigned             x,
                unsigned             y,
                odometerMode_e       mode );
   ~odomValue_t( void );

   enum {
      maxDigits_ = 8
   };

   // initialize to a particular value 
   void set( unsigned pennies );

   // advance by specified number of pixels
   void advance( unsigned numPixels );

   unsigned sigDigits( void ) const { return sigDigits_ ; }

   unsigned value( void ) const { return pennies_ ; }

   rectangle_t const &getRect( void ) const { return r_ ; }

   fbImage_t const &getBackground( void ) const { return background_ ; }
private:
   void drawDigit( unsigned idx );
   void hideDigit( unsigned idx );

   odomGraphics_t const &graphics_ ;
   sm501alpha_t         &alpha_ ;
   odometerMode_e  const mode_ ;
   unsigned              commaPos_ ;
   rectangle_t           r_ ;
   fbImage_t const       background_ ;
   rectangle_t           decimalRect_ ;
   rectangle_t           commaRect_ ;
   rectangle_t           dollarRect_ ;
   odomDigit_t          *digits_[maxDigits_];
   unsigned              sigDigits_ ;
   unsigned              pennies_ ;
};

#endif

