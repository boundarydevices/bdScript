#ifndef __FBCMOVEABLE_H__
#define __FBCMOVEABLE_H__ "$Id: fbcMoveable.h,v 1.2 2006-12-13 21:29:25 ericn Exp $"

/*
 * fbcMoveable.h
 *
 * This header file declares the fbcMoveable class, which is an asynchronous 
 * on-screen object that controls and optimizes a a clear command and a 
 * blt command for either alpha-layer objects or those with a fixed background
 * color. 
 *
 * Change History : 
 *
 * $Log: fbcMoveable.h,v $
 * Revision 1.2  2006-12-13 21:29:25  ericn
 * -updates
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "asyncScreenObj.h"
#include "fbCmdBlt.h"
#include "fbCmdClear.h"
#include "fbCmdWait.h"

class fbcMoveable_t : public asyncScreenObject_t {
public:
   fbcMoveable_t( fbCommandList_t &cmdList,
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
                  unsigned short   backgroundColor = 0 );
   ~fbcMoveable_t( void );

   virtual void executed();
   virtual void updateCommandList();

   inline unsigned getX( void ) const ;   
   inline unsigned getY( void ) const ;   
   
   void setX( unsigned x );
   void setY( unsigned y );
   inline void setPos( unsigned x, unsigned y );

   inline unsigned getWidth( void ) const ;

private:
   fbcMoveable_t( fbcMoveable_t const & ); // no copies


   struct state_t {
      unsigned xPos_ ;
      unsigned yPos_ ;
      inline bool operator==( state_t const &rhs ) const ;
      inline bool operator!=( state_t const &rhs ) const ;
   };

   state_t        onScreenState_ ;
   state_t        commandState_ ;
   state_t        offScreenState_ ;

   fbJump_t      *jump1_ ;
   fbWait_t      *wait1_ ;
   fbCmdClear_t  *clr_ ;
   fbWait_t      *wait2_ ;
   fbBlt_t       *blt_ ;

   unsigned       cmdY_ ;
};

unsigned fbcMoveable_t::getX( void ) const {
   return blt_->getDestX();
}

unsigned fbcMoveable_t::getY( void ) const {
   return cmdY_ ;
}

unsigned fbcMoveable_t::getWidth( void ) const {
   return blt_->getWidth();
}

void fbcMoveable_t::setPos( unsigned x, unsigned y ){
   setX( x ); setY( y );
}

bool fbcMoveable_t::state_t::operator==(state_t const &rhs ) const {
   int diff = diff = xPos_ - rhs.xPos_ ;
   if( 0 == diff )
      diff = yPos_ - rhs.yPos_ ;

   return ( 0 == diff );
}

bool fbcMoveable_t::state_t::operator!=(state_t const &rhs ) const {
   int diff = diff = xPos_ - rhs.xPos_ ;
   if( 0 == diff )
      diff = yPos_ - rhs.yPos_ ;

   return ( 0 != diff );
}


#endif

