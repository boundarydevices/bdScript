/*
 * Module slotWheel.cpp
 *
 * This module defines the methods of the slotWheel_t class
 * as declared in slotWheel.h
 *
 *
 * Change History : 
 *
 * $Log: slotWheel.cpp,v $
 * Revision 1.2  2006-12-13 21:22:33  ericn
 * -updated
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "slotWheel.h"
#include <math.h>

slotWheel_t::slotWheel_t
   ( jsData_t const &motionParams,
     fbCommandList_t &cmdList,
     unsigned long    destRamOffs,    // generally either graphics or alpha RAM
     unsigned         destx,          // screen position
     unsigned         desty,          // screen position
     unsigned         destw,          // screen width
     unsigned         desth,          // screen height
     fbImage_t const &srcImg,         // source image (digit strip)
     unsigned         height )        // visible height
   : worked_( true )
   , circ_( 0 )
   , state_( idle )
   , height_( srcImg.height() )
   , position_( 0 )
   , backSlope_( 0 )
   , bounceDist_( 0 )
   , bounceMag_( 0 )
   , bounceSpeed_( 0 )
   , maxAccel_( 0 )
   , maxBack_( 0 )
   , startPos_( 0 )
   , fAccel_( 0 )
   , fDecel_( 0 )
   , tBackEnd_( 0 )
   , tAccelEnd_( 0 )
   , tDecelEnd_( 0 )
{
   if( !motionParams.getInt( "startPos", startPos_ ) ){
      printf( "Missing startPos\n" );
      worked_ = false ;
   }
   if( !motionParams.getInt( "maxBack", maxBack_ ) ){
      printf( "Missing maxBack\n" );
      worked_ = false ;
   }
   if( !motionParams.getInt( "backSlope", backSlope_ ) ){
      printf( "Missing backSlope\n" );
      worked_ = false ;
   }
   if( !motionParams.getInt( "maxAccel", maxAccel_ ) ){
      printf( "Missing maxAccel\n" );
      worked_ = false ;
   }
   if( !motionParams.getDouble( "fAccel", fAccel_ ) ){
      printf( "Missing fAccel\n" );
      worked_ = false ;
   }
   if( !motionParams.getDouble( "fDecel", fDecel_ ) ){
      printf( "Missing fDecel\n" );
      worked_ = false ;
   }
   if( !motionParams.getInt( "bounceMag", bounceMag_ ) ){
      printf( "Missing bounceMag\n" );
      worked_ = false ;
   }
   if( !motionParams.getInt( "bounceDist", bounceDist_ ) ){
      printf( "Missing bounceDist\n" );
      worked_ = false ;
   }
   if( !motionParams.getInt( "bounceSpeed", bounceSpeed_ ) ){
      printf( "Missing bounceSpeed\n" );
      worked_ = false ;
   }

   circ_ = new fbcCircular_t( cmdList, destRamOffs, destx, desty, destw, desth,
                              srcImg, height, 0, false, 0 );
   tBackEnd_ = maxBack_ ;
   tAccelEnd_ = maxBack_ + maxAccel_ ;

   double invDecel = 0.0 - fDecel_ ;
   tDecelEnd_ = (unsigned)( (double)backSlope_/invDecel + (fAccel_*maxAccel_)/invDecel + tAccelEnd_ );
   printf( "decelEnd == %u\n", tDecelEnd_ );
}

slotWheel_t::~slotWheel_t( void )
{
}

void slotWheel_t::start( unsigned targetOffset )
{
   position_ = 0 ;
   state_ = backup ;
   if( circ_ )
      circ_->show();
}

#define CIRCPOS( __val )( ((int)__val)%height_ )

void slotWheel_t::tick(unsigned long t)
{
   if( circ_ ){
      circ_->executed();
      if( t < tBackEnd_ ){
printf( "backup\n" );
         circ_->setOffset(startPos_+(backSlope_*t));
      } else if( t < tAccelEnd_ ){
         unsigned long pos = (unsigned long)( 
                             startPos_+(backSlope_*t)             // backPos(t)
                           + backSlope_*(t-tBackEnd_)             // v0*t
                           + (0.5*fAccel_*pow(t-tBackEnd_,2))     // 1/2 at**2
                             );
         circ_->setOffset(pos);
printf( "accelerate\n" );
      } else if( t < tDecelEnd_ ){
printf( "decelerate\n" );
      }
      else {
printf( "done\n" );
      }
      circ_->updateCommandList();
   }
}

#ifdef MODULETEST

#include <stdio.h>
#include "memFile.h"
#include "sm501alpha.h"
#include "imgFile.h"
#include "fbDev.h"
#include "fbCmdFinish.h"
#include "fbCmdListSignal.h"
#include "vsyncSignal.h"
#include "multiSignal.h"

struct slotWheelSet_t {
   unsigned     count_ ;
   slotWheel_t *wheels_[3];
};

static fbCmdListSignal_t *cmdListDev_ = 0 ;
static unsigned long     startTick_ ;
static unsigned volatile vsyncCount = 0 ;
static unsigned volatile cmdComplete = 0 ;

static void videoOutput( int signo, void *param )
{
//   printf( "v" ); fflush(stdout);
   fbPtr_t *mem = (fbPtr_t *)param ;

   if( cmdListDev_ && mem ){
      if( vsyncCount == cmdComplete ){
         ++vsyncCount ;
         unsigned offset = mem->getOffs();
         int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
         if( numWritten != sizeof( offset ) ){
            perror( "write(cmdListDev_)" );
            exit(1);
         }
      }
   }
}

static void cmdListComplete( int signo, void *param )
{
   ++cmdComplete ;

   unsigned long syncCount ;
   getFB().syncCount( syncCount );

   unsigned long t = syncCount-startTick_ ;

   slotWheelSet_t *wheels = (slotWheelSet_t *)param ;
   for( unsigned w = 0 ; w < wheels->count_ ; w++ ){
      wheels->wheels_[w]->tick(t);
   }
}

int main( int argc, char const * const argv[] )
{
   if( 2 < argc )
   {
//      sm501alpha_t &alphaLayer = sm501alpha_t::get(sm501alpha_t::rgba4444);

      char const *fileName = argv[1];
      memFile_t fConfig( fileName );
      if( fConfig.worked() ){
         jsData_t cfgData( (char const *)fConfig.getData(), fConfig.getLength(), fileName );
         if( cfgData.initialized() ){
            image_t image ;
            fileName = argv[2];
            if( imageFromFile( fileName, image ) )
            {
               fbDevice_t &fb = getFB();
               fb.clear( 0, 0, 0 );

               fb.syncCount( startTick_ );

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

               fbImage_t fbi( image, fbi.rgb565 );

               fbCommandList_t cmdList ;
               slotWheelSet_t wheels ;
               wheels.count_ = 0 ;

               unsigned xPos = fb.getWidth()/2 - ((3*fbi.width())/2) ;
               unsigned yPos = (fb.getHeight()/2) - fbi.width()/2 ;
               for( unsigned w = 0 ; w < 3 ; w++ ){
                  wheels.count_++ ;
                  wheels.wheels_[w] = new slotWheel_t( cfgData, cmdList, 0,
                                                       xPos, yPos, fb.getWidth(), fb.getHeight(),
                                                       fbi, fbi.width() );
                  wheels.wheels_[w]->start(1);
                  xPos += fbi.width();
               }
               cmdList.push( new fbFinish_t );
         
               fbPtr_t cmdListMem( cmdList.size() );
               cmdList.copy( cmdListMem.getPtr() );

               sigset_t blockThese ;
               sigemptyset( &blockThese );
               
               sigaddset( &blockThese, cmdListDev.getSignal() );
               sigaddset( &blockThese, vsync.getSignal() );
         
               setSignalHandler( vsync.getSignal(), blockThese, videoOutput, &cmdListMem );
               setSignalHandler( cmdListDev.getSignal(), blockThese, cmdListComplete, &wheels );

               cmdListDev_ = &cmdListDev ;
   
               vsync.enable();
               cmdListDev.enable();

               char inBuf[256];
               fgets( inBuf, sizeof(inBuf), stdin );
               
               vsync.disable();
               cmdListDev.disable();
            }
            else
               perror( fileName );
         }
         else
            fprintf( stderr, "%s: %s\n", fileName, cfgData.errorMsg() );
      }
      else
         perror( fileName );
   }
   else
      fprintf( stderr, "Usage: slotWheel dataFile imgFile [x y h]\n" );

   return 0 ;
}
#endif
