#ifndef __ODOMVALUE_H__
#define __ODOMVALUE_H__ "$Id: odomValue.h,v 1.3 2006-10-16 22:25:31 ericn Exp $"

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
 * Revision 1.3  2006-10-16 22:25:31  ericn
 * -use blts for dollar, comma, decimal
 *
 * Revision 1.2  2006/10/10 20:49:23  ericn
 * -bump maxDigits to 10
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
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
#include "fbCmdBlt.h"

class odomValue_t {
public:
   odomValue_t( fbCommandList_t      &cmdList,
                odomGraphics_t const &graphics,
                unsigned             x,
                unsigned             y,
                odometerMode_e       mode );
   ~odomValue_t( void );

   enum {
      maxDigits_ = 10
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
   unsigned        const fbRam_ ; // either alpha or graphics plane offset
   unsigned              commaPos_ ;
   rectangle_t           r_ ;
   fbImage_t      const  background_ ;
   rectangle_t           decimalRect_ ;
   rectangle_t           commaRect_ ;
   rectangle_t           dollarRect_ ;
   odomDigit_t          *digits_[maxDigits_];
   fbBlt_t              *dollarBlt_ ;
   fbBlt_t              *commaBlt_ ;
   fbBlt_t              *decimalBlt_ ;
   unsigned              sigDigits_ ;
   unsigned              pennies_ ;
};

#endif

