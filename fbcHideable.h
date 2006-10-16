#ifndef __FBCHIDEABLE_H__
#define __FBCHIDEABLE_H__ "$Id: fbcHideable.h,v 1.1 2006-10-16 22:45:38 ericn Exp $"

/*
 * fbcHideable.h
 *
 * This header file declares the fbcHideable_t class, which is 
 * an asynchronous on-screen object that controls and optimizes 
 * a blt command and a clear command for either alpha-layer objects 
 * or those with a fixed background color.
 *
 * Change History : 
 *
 * $Log: fbcHideable.h,v $
 * Revision 1.1  2006-10-16 22:45:38  ericn
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

class fbcHideable_t : public asyncScreenObject_t {
public:
   fbcHideable_t( fbCommandList_t &cmdList,
                  unsigned long    destRamOffs,
                  unsigned         destx,
                  unsigned         desty,
                  unsigned         destw,
                  unsigned         desth,
                  fbImage_t const &srcImg,
                  unsigned         srcx,
                  unsigned         srcy,
                  unsigned         w,
                  unsigned         h,
                  bool             initVisible,
                  unsigned short   backgroundColor = 0 );
   ~fbcHideable_t( void );

   virtual void executed();
   virtual void updateCommandList();

   void show( void );
   void hide( void );

   inline unsigned getX(void) const ;

   void dump( void );

private:
   fbcHideable_t( fbcHideable_t const & ); // no copies
   enum state_e {
      unknown_e   = 0,
      visible_e   = 1,
      hidden_e    = 2
   };

   enum action_e {
      blt = 0,
      clear = 1,
      skip  = 2
   };

   state_e        onScreenState_ ;
   state_e        commandState_ ;
   state_e        offScreenState_ ;

   fbJump_t      *jump1_ ;
   fbWait_t      *wait1_ ;
   fbBlt_t       *blt_ ;
   fbJump_t      *jump2_ ;
   fbWait_t      *wait2_ ;
   fbCmdClear_t  *clr_ ;

   unsigned       skipSize_ ; // length of first jump for skip
   unsigned       clearSize_ ; // length of first jump for clear
   action_e       action_ ;
   unsigned       numBlt_ ;
   unsigned       numClear_ ;
   unsigned       numSkip_ ;
};

unsigned fbcHideable_t::getX(void) const {
   return blt_->getDestX();
}

#endif
