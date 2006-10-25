/*
 * Module fbcMoveHide.cpp
 *
 * This module defines the methods of the fbcMoveHide_t
 * class as declared in fbcMoveHide.h
 *
 *
 * Change History : 
 *
 * $Log: fbcMoveHide.cpp,v $
 * Revision 1.2  2006-10-25 23:27:24  ericn
 * -two-step move
 *
 * Revision 1.1  2006/10/16 22:45:39  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbcMoveHide.h"

#include "fbcHideable.h"
#include <linux/sm501-int.h>

fbcMoveHide_t::state_t const fbcMoveHide_t::unknownState_ = {
   state_: unknown_e,
   xPos_: 0
};

fbcMoveHide_t::state_t const fbcMoveHide_t::visibleState_ = {
   state_: visible_e,
   xPos_: 0
};

fbcMoveHide_t::state_t const fbcMoveHide_t::hiddenState_ = {
   state_: hidden_e,
   xPos_: 0
};

fbcMoveHide_t::fbcMoveHide_t
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
     bool             initVisible,
     unsigned short   backgroundColor )
   : asyncScreenObject_t( cmdList )
   , onScreenState_( unknownState_ )
   , commandState_( unknownState_ )
   , offScreenState_( initVisible ? visibleState_ : hiddenState_ )
   , jump1_( new fbJump_t( 0 ) )
   , wait1_( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) )
   , blt_( new fbBlt_t( destRamOffs, destx, desty, destw, desth,
                        srcImg, srcx, srcy, w, h ) )
   , jump2_( new fbJump_t( 0 ) )
   , wait2_( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) )
   , clr_( new fbCmdClear_t( destRamOffs, destx, desty, w, h, backgroundColor ) )
   , action_( initVisible ? blt : clear )
   , numBlt_( 0 )
   , numClear_( 0 )
   , numSkip_( 0 )
{
   offScreenState_.xPos_ = destx ;
   cmdList.push( jump1_ );
   cmdList.push( wait1_ );
   cmdList.push( blt_ );
   jump2_->setLength( wait2_->size() + clr_->size() );
   cmdList.push( jump2_ );
   cmdList.push( wait2_ );
   cmdList.push( clr_ );

   // length of first jump for skip entirely
   skipSize_ = wait1_->size()
             + blt_->size()
             + jump2_->size()
             + wait2_->size()
             + clr_->size();

   // length of first jump to skip blt
   clearSize_ = wait1_->size()
              + blt_->size()
              + jump2_->size();

   updateCommandList();
}

fbcMoveHide_t::~fbcMoveHide_t( void )
{
}

void fbcMoveHide_t::executed()
{
   onScreenState_ = commandState_ ;
   switch( action_ ){
      case blt : ++numBlt_ ; break ;
      case clear : ++numClear_ ; break ;
      case skip  : ++numSkip_ ; break ;
   }
}

void fbcMoveHide_t::updateCommandList()
{
   if( !valueBeingSet() ){
      int prevState = commandState_.state_ ;
      bool changed = false ;
      if( commandState_ != offScreenState_ )
      {
         assert( unknown_e != offScreenState_.state_ );
//         printf( "moveHide: state %d->%d\n", commandState_.state_, offScreenState_.state_ );
         if( ( onScreenState_.xPos_ != offScreenState_.xPos_ )
             &&
             ( onScreenState_.state_ == visible_e ) ){
            commandState_.state_ = hidden_e ;
            commandState_.xPos_  = onScreenState_.xPos_ ;
         } // need to clear old value
         else {
            commandState_ = offScreenState_ ;
         }
         changed = true ;
      }

      if( onScreenState_== commandState_ ){
//         if( changed )
//            printf( "!!! skipped state %d to %d\n", prevState, offScreenState_.state_ );
         jump1_->setLength( skipSize_ );
         action_ = skip ;
      }
      else if( visible_e == commandState_.state_ ){
//         if( changed )
//            printf( "   now visible from %d\n", prevState );
         blt_->setDestX( commandState_.xPos_ );
         jump1_->setLength( 0 );
         action_ = blt ;
      }
      else {
//         if( changed )
//            printf( "   now hidden from %d\n", prevState );
         clr_->setDestX( commandState_.xPos_ );
         jump1_->setLength( clearSize_ );
         action_ = clear ;
      }
   }
}

void fbcMoveHide_t::show( void )
{
   setValueStart();
   offScreenState_.state_ = visible_e ;
   setValueEnd();
}

void fbcMoveHide_t::hide( void )
{
   setValueStart();
   offScreenState_.state_ = hidden_e ;
   setValueEnd();
}

void fbcMoveHide_t::setX( unsigned x ){
   setValueStart();
   offScreenState_.xPos_ = x ;
   setValueEnd();
}

void fbcMoveHide_t::dump( void ){
   printf( "%5u blts\n"
           "%5u clears\n"
           "%5u skips\n",
           numBlt_, numClear_, numSkip_ );
}

#ifdef MODULETEST

#include <stdio.h>
#include "fbCmdListSignal.h"
#include "vsyncSignal.h"
#include "fbCmdFinish.h"
#include "fbCmdWait.h"
#include "imgFile.h"
#include "fbImage.h"
#include "fbDev.h"
#include "hexDump.h"
#include <sys/ioctl.h>
#include <linux/sm501-int.h>
#include <signal.h>
#include "tickMs.h"
#include "multiSignal.h"

static fbPtr_t           *cmdListMem_ = 0 ;
static fbCmdListSignal_t *cmdListDev_ = 0 ;
static unsigned imgCount = 0 ;
static fbcMoveHide_t **objs = 0 ;

static void videoOutput( int signo, void *param )
{
   if( cmdListMem_ && cmdListDev_ ){
      unsigned offset = cmdListMem_->getOffs();
      int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
      if( numWritten != sizeof( offset ) ){
         perror( "write(cmdListDev_)" );
         exit(1);
      }
   }
}

static void cmdListComplete( int signo, void *param )
{
   for( unsigned i = 0 ; i < imgCount ; i++ ){
      objs[i]->executed();
      objs[i]->updateCommandList();
   }
}

int main( int argc, char const * const argv[] )
{
   if( 1 < argc )
   {
      fbDevice_t &fb = getFB();

      sigset_t blockThese ;
      sigemptyset( &blockThese );

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
      
      fbCommandList_t cmdList ;
      bool visible = true ;

      imgCount = argc-1 ;
      fbImage_t ** const images = new fbImage_t *[imgCount];
      objs = new fbcMoveHide_t *[imgCount];
      unsigned   * const offsets = new unsigned [imgCount];

      unsigned x = 0 ;
      unsigned fbRAM = 0 ; 

      for( int arg = 1 ; arg < argc ; arg++ )
      {
         image_t image ;
         if( !imageFromFile( argv[arg], image ) ){
            perror( argv[arg] );
            return -1 ;
         }

         fbImage_t *const newImg = new fbImage_t( image, fbImage_t::rgb565 );

         printf( "%u x %u pixels, stride %u\n", image.width_, image.height_, newImg->stride() );

         unsigned const i = arg-1 ;
         images[i] = newImg ;
         offsets[i] = cmdList.size();
         objs[i] = new fbcMoveHide_t( cmdList, fbRAM, x, 0, fb.getWidth(), fb.getHeight(), *newImg,
                                      0, 0, newImg->width(), newImg->height(),
                                      visible, 0x001f );
         visible = !visible ;
         x += newImg->width();
      }
      
      printf( "loaded %u images\n", argc-1 );
      cmdList.push( new fbFinish_t );
      printf( "%u bytes of cmdlist\n", cmdList.size() );

      fbPtr_t cmdListMem( cmdList.size() );
      cmdList.copy( cmdListMem.getPtr() );
      hexDumper_t dump( cmdListMem.getPtr(), cmdListMem.size(), cmdListMem.getOffs() );
      while( dump.nextLine() )
         printf( "%s\n", dump.getLine() );

      cmdListMem_ = &cmdListMem ;
      cmdListDev_ = &cmdListDev ;

      sigaddset( &blockThese, cmdListDev.getSignal() );
      sigaddset( &blockThese, vsync.getSignal() );

      setSignalHandler( vsync.getSignal(), blockThese, videoOutput, 0 );
      setSignalHandler( cmdListDev.getSignal(), blockThese, cmdListComplete, 0 );

      vsync.enable();
      cmdListDev.enable();

      int rval = ioctl( fb.getFd(), SM501_EXECCMDLIST, cmdListMem.getOffs() );
      if( 0 == rval ){
         printf( "executed\n" );
         long long const startTick = tickMs();
         long long prevTick = startTick ;
         while( 5000 > (prevTick-startTick) ){
            pause();
            if( 1000 < ( tickMs()-prevTick ) ){
               prevTick = tickMs();
               for( unsigned i = 0 ; i < imgCount; i++ ){
                  if( visible ){
                     objs[i]->show();
                     objs[i]->setX( objs[i]->getX() + 2*images[0]->width() );
                  }
                  else
                     objs[i]->hide();
                  visible = !visible ;
               }
            }
         }
         for( unsigned i = 0 ; i < imgCount ; i++ ){
            printf( "--- file %s\n", argv[i+1] );
            objs[i]->dump();
         }
      }
      else {
         printf( "result: %d\n", rval );
      }
   }
   else
      fprintf( stderr, "Usage: %s imgFile [imgFile...]\n", argv[0] );

   return 0 ;
}

#endif

