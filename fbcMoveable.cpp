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
 * Revision 1.1  2006-11-09 17:09:01  ericn
 * -Initial import
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
   , srcImage_( srcImg )
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
   commandState_.xPos_ = destx ;
   commandState_.yPos_ = desty ;
   offScreenState_ = commandState_ ;

   cmdList.push( jump1_ );
   cmdList.push( wait1_ );
   cmdList.push( clr_ );
   cmdList.push( wait2_ );
   cmdList.push( blt_ );

   // length of first jump to skip entirely
   skipSize_ = wait1_->size()
             + clr_->size()
             + wait2_->size()
             + blt_->size();

   updateCommandList();
}

fbcMoveable_t::~fbcMoveable_t( void )
{
}

void fbcMoveable_t::executed()
{
   onScreenState_ = commandState_ ;
}

void fbcMoveable_t::updateCommandList()
{
   if( !valueBeingSet() ){
      if( commandState_ != offScreenState_ )
      {
         clr_->setDestX( commandState_.xPos_ );
         clr_->setDestRamOffs( blt_->getDestRamOffs() );

         commandState_ = offScreenState_ ;

         blt_->setDestX( commandState_.xPos_ );

         int yDiff = commandState_.yPos_-cmdY_ ;
         blt_->moveDestY( yDiff );
         cmdY_ += yDiff ;
      } // change requested

      if( onScreenState_== commandState_ ){
         jump1_->setLength( skipSize_ );
      }
      else {
         jump1_->setLength( 0 );
      }
   } // mutual exclusion
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

void fbcMoveable_t::jumpTo( unsigned x, unsigned y )
{
   setValueStart();

   commandState_.xPos_ = x ;
   blt_->setDestX( x );

   int yDiff = y-cmdY_ ;
   blt_->moveDestY( yDiff );
   cmdY_ += yDiff ;

   clr_->setDestX( commandState_.xPos_ );
   clr_->setDestRamOffs( blt_->getDestRamOffs() );

   setValueEnd();
}

void fbcMoveable_t::setSourceOrigin( unsigned srcX, unsigned srcY )
{
   blt_->setSourceRamOffs( srcImage_.ramOffset() + srcX*2 + srcY*srcImage_.strideBytes() );
}

#ifdef MODULETEST_MOVEABLE
#include <stdio.h>
#include "imgFile.h"
#include "fbCmdFinish.h"
#include "fbCmdListSignal.h"
#include "vsyncSignal.h"
#include "multiSignal.h"

#define IMGFILENAME "/mmc/elements.png"
static fbCmdListSignal_t *cmdListDev_ = 0 ;
static unsigned cmdListOffs_ = 0 ;
static unsigned volatile vsyncCount = 0 ;
static unsigned volatile cmdComplete = 0 ;
static bool volatile done = false ;

static void videoOutput( int signo, void *param )
{
   assert( cmdListDev_ );

   if( ( vsyncCount == cmdComplete ) && !done ){
      int numWritten = write( cmdListDev_->getFd(), &cmdListOffs_, sizeof( cmdListOffs_ ) );
      assert( numWritten == sizeof( cmdListOffs_ ) );
      vsyncCount++ ;
   }
}

static void cmdListComplete( int signo, void *param )
{
   ++cmdComplete ;
   fbcMoveable_t &moveable = *(fbcMoveable_t *)param ;

   moveable.executed();

   unsigned xPos = moveable.getX();
   if( 0 < xPos )
      moveable.setX( xPos-1 );
   unsigned yPos = moveable.getY();
   if( 0 < yPos )
      moveable.setY( yPos-1 );
   if( ( 0 == xPos ) && ( 0 == yPos ) )
      done = true ;

   printf( "%u:%u\n", xPos, yPos );
   moveable.updateCommandList();
}

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   done = true ;
}


int main( int argc, char const * const argv[] )
{
   image_t image ;
   if( imageFromFile( IMGFILENAME, image ) )
   {
      vsyncSignal_t &vsync = vsyncSignal_t::get();
      if( !vsync.isOpen() ){
         perror( "vsyncSignal" );
         return -1 ;
      }

      fbCmdListSignal_t &cmdListDev = fbCmdListSignal_t::get();
      if( !cmdListDev.isOpen() ){
         perror( "cmdListDev" );
         return -1 ;
      }
      cmdListDev_ = &cmdListDev ;

      fbDevice_t &fb = getFB();

      fbImage_t fbi( image, fbi.rgb565 );
      printf( "image %ux%u\n", fbi.width(), fbi.height() );

      fbCommandList_t cmdList ;
      fbcMoveable_t *moveable = new fbcMoveable_t
         ( cmdList,
           0, 
           fb.getWidth()/2, fb.getHeight()/2,
           fb.getWidth(), fb.getHeight(),
           fbi,
           0, 256,
           256, 256,
           0 );

      cmdList.push( new fbFinish_t );
      fbPtr_t cmdMem( cmdList.size() );
      cmdList.copy( cmdMem.getPtr() );

      cmdListOffs_ = cmdMem.getOffs();

      sigset_t blockThese ;
      sigemptyset( &blockThese );

      sigaddset( &blockThese, cmdListDev.getSignal() );
      sigaddset( &blockThese, vsync.getSignal() );

      setSignalHandler( vsync.getSignal(), blockThese, videoOutput, 0 );
      setSignalHandler( cmdListDev.getSignal(), blockThese, cmdListComplete, moveable );

      vsync.enable();
      cmdListDev.enable();

      signal( SIGINT, ctrlcHandler );

      while( !done )
         pause();
   
      vsync.disable();
      cmdListDev.disable();

   }
   else
      perror( IMGFILENAME );

   return 0 ;
}
#endif
