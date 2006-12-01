/*
 * Module fbcHideable.cpp
 *
 * This module defines the methods of the fbcHideable_t
 * class as declared in fbcHideable.h
 *
 * Implementation consists of six commands:
 *
 *    jump              (to clear or end)
 *    wait for drawing engine
 *    blt
 *    jump              (skip clear)
 *    wait for drawing engine
 *    clear
 * 
 * Change History : 
 *
 * $Log: fbcHideable.cpp,v $
 * Revision 1.2  2006-12-01 22:49:50  tkisky
 * -include assert
 *
 * Revision 1.1  2006/10/16 22:45:37  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbcHideable.h"
#include <linux/sm501-int.h>
#include <assert.h>

fbcHideable_t::fbcHideable_t
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
   , onScreenState_( unknown_e )
   , commandState_( unknown_e )
   , offScreenState_( initVisible ? visible_e : hidden_e )
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

fbcHideable_t::~fbcHideable_t( void )
{
}

void fbcHideable_t::executed()
{
   onScreenState_ = commandState_ ;
   switch( action_ ){
      case blt : ++numBlt_ ; break ;
      case clear : ++numClear_ ; break ;
      case skip  : ++numSkip_ ; break ;
   }
}

void fbcHideable_t::updateCommandList()
{
   if( !valueBeingSet() ){
      if( commandState_ != offScreenState_ )
      {
         assert( unknown_e != offScreenState_ );
         commandState_ = offScreenState_ ;
      }
      
      if( onScreenState_== commandState_ ){
         jump1_->setLength( skipSize_ );
         action_ = skip ;
      }
      else if( visible_e == commandState_ ){
         jump1_->setLength( 0 );
         action_ = blt ;
      }
      else {
         jump1_->setLength( clearSize_ );
         action_ = clear ;
      }
   }
}

void fbcHideable_t::show( void )
{
   setValueStart();
   offScreenState_ = visible_e ;
   setValueEnd();
}

void fbcHideable_t::hide( void )
{
   setValueStart();
   offScreenState_ = hidden_e ;
   setValueEnd();
}

void fbcHideable_t::dump( void ){
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
static fbcHideable_t **objs = 0 ;

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
      objs = new fbcHideable_t *[imgCount];
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
         objs[i] = new fbcHideable_t( cmdList, fbRAM, x, 0, fb.getWidth(), fb.getHeight(), *newImg,
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

