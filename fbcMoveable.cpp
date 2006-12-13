/*
 * Module fbcMoveable.cpp
 *
 * This module defines the methods of the fbcMoveable_t
 * class as declared in fbcMoveable.h
 *
 *
 * Change History : 
 *
 * $Log: fbcMoveable.cpp,v $
 * Revision 1.2  2006-12-13 21:29:19  ericn
 * -updates
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbcMoveable.h"

fbcMoveable_t::fbcMoveable_t
   ( fbCommandList_t &cmdList,
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
     unsigned short   backgroundColor )
   : asyncScreenObject_t( cmdList )
   , onScreenState_()
   , commandState_()
   , offScreenState_()
   , jump1_( new fbJump_t( 0 ) )
   , wait1_( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) )
   , clr_( new fbCmdClear_t( destRamOffs, destx, desty, w, h, backgroundColor ) )
   , wait2_( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) )
   , blt_( new fbBlt_t( destRamOffs, destx, desty, destw, desth,
                        srcImg, srcx, srcy, w, h ) )
   , cmdY_(desty)
{
   onScreenState_.xPos_ = ~destx ;
   onScreenState_.yPos_ = ~desty ;
   commandState_ = onScreenState_ ;
   offScreenState_.xPos_ = destx ;
   offScreenState_.yPos_ = desty ;
}

fbcMoveable_t::~fbcMoveable_t( void )
{
}

void fbcMoveable_t::executed()
{
}

void fbcMoveable_t::updateCommandList()
{
}

void fbcMoveable_t::setX( unsigned x ){
   setValueStart();
   offScreenState_.xPos_ = x ;
   setValueEnd();
}

void fbcMoveable_t::setY( unsigned y ){
   setValueStart();
   offScreenState_.yPos_ = y ;
   setValueEnd();
}

/*
   state_t        onScreenState_ ;
   state_t        commandState_ ;
   state_t        offScreenState_ ;

   fbJump_t      *jump1_ ;
   fbWait_t      *wait1_ ;
   fbCmdClear_t  *clr_ ;
   fbWait_t      *wait2_ ;
   fbBlt_t       *blt_ ;

   unsigned       cmdY_ ;
*/

