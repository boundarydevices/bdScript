/*
 * Module fbcCircular.cpp
 *
 * This module defines the methods of the fbcCircular_t
 * class as declared in fbcCircular.h.
 *
 * Implementation consists of eight commands:
 *
 *    jump              (to blt2, clear, or end)
 *    wait for drawing engine
 *    blt1              (top of image)
 *    wait for drawing engine
 *    blt2              (bottom of image)
 *    jump              (skip clear)
 *    wait for drawing engine
 *    clear
 *
 * The first jump goes to one of:
 *    blt1        - if two blts are necessary
 *    blt2        - if a single blt is enough
 *    clear       - to clear the display
 *    end         - if the on-screen display is up-to-date
 *
 * The second jump always jumps to the end.
 *
 * Change History : 
 *
 * $Log: fbcCircular.cpp,v $
 * Revision 1.1  2006-10-16 22:45:35  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbcCircular.h"

#include <linux/sm501-int.h>

static fbcCircular_t::state_t const unknownState_ = { 
      displayState_: fbcCircular_t::unknown_e
,     offset_: 0
};

static fbcCircular_t::state_t const visibleState_ = { 
      displayState_: fbcCircular_t::visible_e
,     offset_: 0
};

static fbcCircular_t::state_t const hiddenState_ = { 
      displayState_: fbcCircular_t::hidden_e
,     offset_: 0
};

unsigned volatile flags = 0 ;

fbcCircular_t::fbcCircular_t
   ( fbCommandList_t &cmdList,
     unsigned long    destRamOffs,
     unsigned         destx,
     unsigned         desty,
     unsigned         destw,
     unsigned         desth,
     fbImage_t const &srcImg,
     unsigned         height,
     unsigned         yPixelOffs,
     bool             initVisible,
     unsigned short   backgroundColor )
   : asyncScreenObject_t( cmdList )
   , ramOffs_( destRamOffs )
   , destx_( destx )
   , desty_( desty )
   , destw_( destw )
   , desth_( desth )
   , srcImg_( srcImg )
   , height_( height )
   , imgHeight_( srcImg.height() )
   , endPoint_( imgHeight_-height )
   , onScreenState_( unknownState_ )
   , commandState_( unknownState_ )
   , offScreenState_( initVisible ? visibleState_ : hiddenState_ )
   , jump1_( new fbJump_t( 0 ) )
   , wait1_( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) )
   , blt1_( new fbBlt_t( destRamOffs, destx, desty, destw, desth,
                         srcImg, 0, yPixelOffs, srcImg.width(), height ) )
   , wait2_( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) )
   , blt2_( new fbBlt_t( destRamOffs, destx, desty, destw, desth,
                         srcImg, 0, yPixelOffs, srcImg.width(), height ) )
   , jump2_( new fbJump_t( 0 ) )
   , wait3_( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) )
   , clr_( new fbCmdClear_t( destRamOffs, destx, desty, srcImg.width(), height, backgroundColor ) )
   , action_( initVisible ? blt1 : clear )
   , numBlt1_( 0 )
   , numBlt2_( 0 )
   , numClear_( 0 )
   , numSkip_( 0 )
{
   assert( srcImg.height() >= height );

   cmdList.push( jump1_ );
   cmdList.push( wait1_ );
   cmdList.push( blt1_ );
   cmdList.push( wait2_ );
   cmdList.push( blt2_ );
   jump2_->setLength( wait2_->size() + clr_->size() );
   cmdList.push( jump2_ );
   cmdList.push( wait3_ );
   cmdList.push( clr_ );

   // length of first jump for skip entirely
   skipSize_ = wait1_->size()
             + blt1_->size()
             + wait2_->size()
             + blt2_->size()
             + jump2_->size()
             + wait3_->size()
             + clr_->size();

   // length to skip first blt
   singleBltSize_ = wait1_->size()
                  + blt1_->size();

   // length of first jump to skip blt
   clearSize_ = wait1_->size()
              + blt1_->size()
              + wait2_->size()
              + blt2_->size()
              + jump2_->size();

   updateCommandList();
}

fbcCircular_t::~fbcCircular_t( void )
{
}

void fbcCircular_t::executed()
{
   assert( 0 == flags );
   flags |= 1 ;
   if( onScreenState_ != commandState_ ){
// printf( "circular: action %d, states %d->%d, offset %u\n", action_, onScreenState_.displayState_, commandState_.displayState_, onScreenState_.offset_ );
      onScreenState_ = commandState_ ;
   }
   switch( action_ ){
      case blt1 : ++numBlt1_ ; break ;
      case blt2 : ++numBlt2_ ; break ;
      case clear : ++numClear_ ; break ;
      case skip  : ++numSkip_ ; break ;
   }
   flags &= ~1 ;
   assert( 0 == flags );
}

void fbcCircular_t::updateCommandList()
{
   assert( 0 == flags );
   flags |= 2 ;
   if( !valueBeingSet() ){
      if( commandState_ != offScreenState_ )
      {
         assert( unknown_e != offScreenState_.displayState_ );
         commandState_ = offScreenState_ ;
      }

      if( onScreenState_== commandState_ ){
         jump1_->setLength( skipSize_ );
         action_ = skip ;
      }
      else if( visible_e == commandState_.displayState_ ){
         if( commandState_.offset_ > endPoint_ ){
            action_ = blt2 ;
            jump1_->setLength( 0 );
            unsigned const firstBltHeight = imgHeight_-commandState_.offset_ ;
            blt1_->set( ramOffs_, 
                        destx_, 
                        desty_,
                        destw_,
                        desth_,
                        srcImg_,
                        0, 
                        commandState_.offset_,
                        srcImg_.width(),
                        firstBltHeight );
// printf( "blt last %u pixels from top\n", height_-firstBltHeight );
            blt2_->set( ramOffs_, 
                        destx_, 
                        desty_ + firstBltHeight,
                        destw_,
                        desth_,
                        srcImg_,
                        0, 0,
                        srcImg_.width(),
                        height_-firstBltHeight );
         }
         else {
            jump1_->setLength( singleBltSize_ );
            blt2_->set( ramOffs_, 
                        destx_, 
                        desty_,
                        destw_,
                        desth_,
                        srcImg_,
                        0, commandState_.offset_,
                        srcImg_.width(),
                        height_ );
            action_ = blt1 ;
         }
      }
      else {
         jump1_->setLength( clearSize_ );
         action_ = clear ;
      }
   }
   flags &= ~2 ;
   assert( 0 == flags );
}

unsigned fbcCircular_t::setOffset( unsigned pixels )
{
   unsigned leftover ;
   setValueStart();
   offScreenState_.offset_ = pixels % imgHeight_ ;
   if( offScreenState_.offset_ > endPoint_ ){
      leftover = offScreenState_.offset_ + height_ - imgHeight_ ;
   }
   else
      leftover = 0 ;
   setValueEnd();
   return leftover ;
}

void fbcCircular_t::show( void )
{
   setValueStart();
   offScreenState_.displayState_ = visible_e ;
   setValueEnd();
}

void fbcCircular_t::hide( void )
{
   setValueStart();
   offScreenState_.displayState_ = hidden_e ;
   setValueEnd();
}

void fbcCircular_t::dump( void ){
   printf( "%5u single blts\n"
           "%5u double blts\n"
           "%5u clears\n"
           "%5u skips\n",
           numBlt1_, numBlt2_, numClear_, numSkip_ );
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
static fbcCircular_t **objs = 0 ;
static unsigned vsyncCount = 0 ;

static void videoOutput( int signo, void *param )
{
   if( cmdListMem_ && cmdListDev_ ){
      ++vsyncCount ;
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
      objs = new fbcCircular_t *[imgCount];
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
         objs[i] = new fbcCircular_t( cmdList, fbRAM, x, 0, 
                                      fb.getWidth(), fb.getHeight(), 
                                      *newImg,
                                      100, 0, visible, 0x001f );
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
         while( 50000 > (prevTick-startTick) ){
            pause();
            if( false ){ // 1000 < ( tickMs()-prevTick ) ){
               for( unsigned i = 0 ; i < imgCount; i++ ){
                  if( visible )
                     objs[i]->show();
                  else
                     objs[i]->hide();
                  visible = !visible ;
               }
               prevTick = tickMs();
               if( visible )
                  prevTick -= 900 ;
            }
            for( unsigned i = 0 ; i < imgCount; i++ ){
               objs[i]->setOffset(3*vsyncCount);
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

