#ifndef __FBCMOVEHIDE_H__
#define __FBCMOVEHIDE_H__ "$Id: fbcMoveHide.h,v 1.4 2002-12-15 05:44:21 ericn Exp $"

/*
 * fbcMoveHide.h
 *
 * This header file declares the fbcMoveHide class, which is an asynchronous 
 * on-screen object that controls and optimizes a blt command and a clear 
 * command for either alpha-layer objects or those with a fixed background 
 * color. 
 *
 * It differ from the fbcHideable_t class in that it can keep track of moves
 * in the x direction.
 *
 * Change History : 
 *
 * $Log: fbcMoveHide.h,v $
 * Revision 1.4  2002-12-15 05:44:21  ericn
 * -added swapSource() method
 *
 * Revision 1.3  2006/10/25 23:27:41  ericn
 * -two-step move
 *
 * Revision 1.2  2006/10/19 03:10:09  ericn
 * -two hidden objects are equal
 *
 * Revision 1.1  2006/10/16 22:45:40  ericn
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

class fbcMoveHide_t : public asyncScreenObject_t {
public:
   fbcMoveHide_t( fbCommandList_t &cmdList,
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
   ~fbcMoveHide_t( void );

   virtual void executed();
   virtual void updateCommandList();

   void show( void );
   void hide( void );

   inline unsigned getX( void ) const ;   
   
   // When changing X-positions, the fbcMoveHide class
   // normally performs a two-step process:
   //       clear old rectangle, then
   //       blt to new rectangle
   // If the application knows that the old position 
   // will be overwritten, this routine can force the
   // clear to be skipped.
   inline void skipHide( void );
   void setX( unsigned x );
   inline unsigned getWidth( void ) const ;

   void swapSource( fbImage_t const &img, unsigned srcy );

   void dump( void );

private:
   fbcMoveHide_t( fbcMoveHide_t const & ); // no copies

   enum state_e {
      unknown_e   = 0,
      visible_e   = 1,
      hidden_e    = 2
   };

   struct state_t {
      state_e  state_ ;
      unsigned xPos_ ;
      inline bool operator==( state_t const &rhs ) const ;
      inline bool operator!=( state_t const &rhs ) const ;
   };

   enum action_e {
      blt = 0,
      clear = 1,
      skip  = 2
   };

   state_t        onScreenState_ ;
   state_t        commandState_ ;
   state_t        offScreenState_ ;

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

   // used to initialize state values in constructor
   static state_t const unknownState_ ;
   static state_t const visibleState_ ;
   static state_t const hiddenState_ ;
};


unsigned fbcMoveHide_t::getX( void ) const {
   return blt_->getDestX();
}

unsigned fbcMoveHide_t::getWidth( void ) const {
   return blt_->getWidth();
}

bool fbcMoveHide_t::state_t::operator==(state_t const &rhs ) const {
   int diff = state_ - rhs.state_ ;
   if( ( 0 == diff ) && ( hidden_e != state_ ) ){
      diff = xPos_ - rhs.xPos_ ;
   }

   return ( 0 == diff );
}

bool fbcMoveHide_t::state_t::operator!=(state_t const &rhs ) const {
   int diff = state_ - rhs.state_ ;
   if( ( 0 == diff ) && ( hidden_e != state_ ) ){
      diff = xPos_ - rhs.xPos_ ;
   }

   return ( 0 != diff );
}

void fbcMoveHide_t::skipHide( void )
{
   onScreenState_.state_ = hidden_e ; // so clear won't occur
}

#endif

