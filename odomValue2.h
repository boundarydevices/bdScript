#ifndef __ODOMVALUE2_H__
#define __ODOMVALUE2_H__ "$Id: odomValue2.h,v 1.3 2002-12-15 08:39:06 ericn Exp $"

/*
 * odomValue2.h
 *
 * This header file declares the odomValue2_t class, which
 * represents an on-screen odometer.
 *
 * Change History : 
 *
 * $Log: odomValue2.h,v $
 * Revision 1.3  2002-12-15 08:39:06  ericn
 * -Added width and height parameters, support scaling of images to fit
 *
 * Revision 1.2  2006/10/19 00:36:25  ericn
 * -move dollar sign to the end, add convenience methods
 *
 * Revision 1.1  2006/10/16 22:45:45  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbcHideable.h"
#include "fbcMoveHide.h"
#include "fbcCircular.h"
#include "odomGraphics.h"

class odomValue2_t {
public:
   odomValue2_t( fbCommandList_t      &cmdList,
                 odomGraphics_t const &graphics,
                 unsigned             x,
                 unsigned             y,
                 unsigned             width = 0,
                 unsigned             height = 0 );
   ~odomValue2_t( void );

   enum {
      maxDigits_ = 10
   };

   // initialize to a particular value 
   void set( unsigned char digits[maxDigits_],
             unsigned      extraPixels );

   void show();
   void hide();

   // call when command list completes execution
   void executed();
   void updateCommandList();

   void dump( void );

   inline unsigned digitHeight( void ) const { return digitHeight_ ; }

public:
   void hideEm(void);
   void showEm( unsigned msd );

   odomGraphics_t const &graphics_ ;
   sm501alpha_t         &alpha_ ;
   unsigned        const fbRam_ ; // either alpha or graphics plane offset
   unsigned        const digitHeight_ ;
   unsigned        const xLeft_ ;
   unsigned              width_ ;

   fbcMoveHide_t        *comma1_ ;
   fbcMoveHide_t        *comma2_ ;
   fbcMoveHide_t        *decimalPoint_ ;
   fbcCircular_t        *digits_[maxDigits_];

   // dollar sign goes last so it's on the top
   fbcMoveHide_t        *dollarSign_ ;
   unsigned char         digitValues_[maxDigits_];
   bool                  wantHidden_ ; // off-screen value
   bool                  cmdHidden_ ; // command value
   bool                  isHidden_ ;  // on-screen value
   unsigned              sigDigits_ ; // number of significant digits
   unsigned              prevSig_ ; // number of significant digits in previous command
   bool                  updating_ ;
};

#endif

