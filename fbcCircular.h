#ifndef __FBCCIRCULAR_H__
#define __FBCCIRCULAR_H__ "$Id: fbcCircular.h,v 1.2 2006-10-19 03:09:52 ericn Exp $"

/*
 * fbcCircular.h
 *
 * This header file declares the fbcCircular_t class, which
 * is an asynchronous on-screen object that supports display
 * of N-out-of-M pixels within an image. 
 * 
 * It currently only supports vertical rotation for use in 
 * constructing odometers or jackpot wheels.
 *
 * This class supports hiding (for use in more-significant
 * odometer digits).
 *
 * Change History : 
 *
 * $Log: fbcCircular.h,v $
 * Revision 1.2  2006-10-19 03:09:52  ericn
 * -two hidden objects are equal
 *
 * Revision 1.1  2006/10/16 22:45:36  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "asyncScreenObj.h"
#include "fbCmdBlt.h"
#include "fbCmdClear.h"
#include "fbCmdWait.h"
#include <string.h>

class fbcCircular_t : public asyncScreenObject_t {
public:
   fbcCircular_t( fbCommandList_t &cmdList,
                  unsigned long    destRamOffs,    // generally either graphics or alpha RAM
                  unsigned         destx,          // screen position
                  unsigned         desty,          // screen position
                  unsigned         destw,          // screen width
                  unsigned         desth,          // screen height
                  fbImage_t const &srcImg,         // source image (digit strip)
                  unsigned         height,         // visible height
                  unsigned         yPixelOffs = 0, // start position
                  bool             initVisible = false,
                  unsigned short   backgroundColor = 0 );
   ~fbcCircular_t( void );

   virtual void executed();
   virtual void updateCommandList();

   inline unsigned getOffset( void ) const { return offScreenState_.offset_ ; }
   
   // returns the number of pixels past the end drawing extends
   unsigned setOffset( unsigned pixels );

   void show( void );
   void hide( void );

   inline unsigned getDestX( void ) const { return blt1_->getDestX(); }

   void dump( void );

private:
   fbcCircular_t( fbcCircular_t const & ); // no copies

public:
   enum state_e {
      unknown_e   = 0,
      visible_e   = 1,
      hidden_e    = 2
   };

   struct state_t {
      state_e  displayState_ ;
      unsigned offset_ ;
      inline bool operator==( state_t const &rhs ) const ;
      inline bool operator!=( state_t const &rhs ) const ;
   };

   enum action_e {
      blt1  = 0,
      blt2  = 1,
      clear = 2,
      skip  = 3
   };

private:
   unsigned const   ramOffs_ ;
   unsigned const   destx_ ;
   unsigned const   desty_ ;
   unsigned const   destw_ ;
   unsigned const   desth_ ;
   fbImage_t const &srcImg_ ;
   unsigned const   height_ ;
   unsigned const   imgHeight_ ;
   unsigned const   endPoint_ ; // display > this requires two blts
   state_t        onScreenState_ ;
   state_t        commandState_ ;
   state_t        offScreenState_ ;

   fbJump_t      *jump1_ ;
   fbWait_t      *wait1_ ;
   fbBlt_t       *blt1_ ;
   fbWait_t      *wait2_ ;
   fbBlt_t       *blt2_ ;
   fbJump_t      *jump2_ ;
   fbWait_t      *wait3_ ;
   fbCmdClear_t  *clr_ ;

   unsigned       skipSize_ ;       // length of first jump to skip entire thing
   unsigned       singleBltSize_ ;  // length of first jump to skip entire thing
   unsigned       clearSize_ ;      // length of first jump for clear
   action_e       action_ ;
   unsigned       numBlt1_ ;
   unsigned       numBlt2_ ;
   unsigned       numClear_ ;
   unsigned       numSkip_ ;
};


bool fbcCircular_t::state_t::operator==( state_t const &rhs ) const 
{
   int diff = displayState_ - rhs.displayState_ ;
   if( ( 0 == diff ) && ( hidden_e != displayState_ ) ){
      diff = offset_ - rhs.offset_ ;
   }
   return 0 == diff ;
}

bool fbcCircular_t::state_t::operator!=( state_t const &rhs ) const 
{
   int diff = displayState_ - rhs.displayState_ ;
   if( ( 0 == diff ) && ( hidden_e != displayState_ ) ){
      diff = offset_ - rhs.offset_ ;
   }
   return 0 != diff ;
}

#endif

