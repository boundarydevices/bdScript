#ifndef __ODOMDIGIT_H__
#define __ODOMDIGIT_H__ "$Id: odomDigit.h,v 1.2 2006-08-16 02:32:22 ericn Exp $"

/*
 * odomDigit.h
 *
 * This header file declares the odomDigit_t class, 
 * which represents an on-screen digit updated via
 * a command-list. It outputs two wait for drawing-engine
 * commands and two blt commands into a command list, 
 * keeping a references to the blt commands for later 
 * update with new values.
 *
 * Change History : 
 *
 * $Log: odomDigit.h,v $
 * Revision 1.2  2006-08-16 02:32:22  ericn
 * -support alpha mode
 *
 * Revision 1.1  2006/08/06 13:19:32  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbCmdList.h"
#include "fbImage.h"
#include "fbCmdBlt.h"
#include "fbDev.h"
#include "odomMode.h"

class odomDigit_t {
public:
   odomDigit_t( fbCommandList_t  &cmdList,
                fbImage_t const  &digitStrip,
                unsigned          x,
                unsigned          y,
                odometerMode_e    mode );
   ~odomDigit_t( void );

   //
   // Update the command(s) to re-draw the digit advanced 
   // by 'howMany' pixels.
   //
   // Returns the number of pixels the next more-significant
   // digit should advance (equal to the number of pixels 
   // advanced between the '9' and '0' position)
   //
   unsigned long advance( unsigned long howMany ); // pixels
   
   inline unsigned pixelOffset( void ) const ;
   inline unsigned digitValue( unsigned &fraction ) const ;

   void     show( void );
   void     hide( void );

   inline bool isVisible( void ) const { return !hidden_ ; }
   inline bool isHidden( void ) const { return hidden_ ; }

   // use only during initial display
   // value is integer digit value [0..9]
   void set( unsigned digitVal );

   rectangle_t const &getRect( void ) const { return r_ ; }
   unsigned getX( void ) const { return r_.xLeft_ ; }

private:
   // shared routine between set() and advance()
   void update( void ); 
   
   fbImage_t const &digitStrip_ ;
   unsigned const   fbOffs_ ; // either 0 (graphics layer) or alpha layer offset
   rectangle_t      r_ ;
   odometerMode_e   mode_ ;
   unsigned const   pos9_ ;
   unsigned         pixelOffs_ ;
   fbImage_t const  background_ ;
   unsigned         bltIdx_[2];
   fbBlt_t         *blts_[2];
   bool             hidden_ ;
};




unsigned odomDigit_t::pixelOffset( void ) const 
{ 
   return pixelOffs_ ; 
}

unsigned odomDigit_t::digitValue( unsigned &fraction ) const 
{
   fraction = pixelOffs_ % r_.height_ ;
   return pixelOffs_ / r_.height_ ;
}

#endif
