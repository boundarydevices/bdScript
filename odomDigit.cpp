/*
 * Module odomDigit.cpp
 *
 * This module defines the methods of the odomDigit_t
 * class, as declared in odomDigit.h
 *
 *
 * Change History : 
 *
 * $Log: odomDigit.cpp,v $
 * Revision 1.2  2006-08-16 02:32:31  ericn
 * -support alpha mode
 *
 * Revision 1.1  2006/08/06 13:19:34  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomDigit.h"
#include "fbCmdBlt.h"
#include "fbCmdWait.h"
#include "fbDev.h"
#include "sm501alpha.h"

odomDigit_t::odomDigit_t( 
   fbCommandList_t  &cmdList,
   fbImage_t const  &digitStrip,
   unsigned          x,
   unsigned          y,
   odometerMode_e    mode )
   : digitStrip_( digitStrip )
   , fbOffs_( (mode==graphicsLayer)
               ? 0
               : sm501alpha_t::get(sm501alpha_t::rgba4444).fbRamOffset() )
   , r_( makeRect(x,y,digitStrip_.width(), digitStrip_.height()/10) )
   , mode_( mode )
   , pos9_( 9*r_.height_ )
   , pixelOffs_( 0 )
   , background_( x, y, r_.width_, r_.height_ )
   , hidden_( false )
{
   fbDevice_t &fb = getFB();

   cmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
   bltIdx_[0] = cmdList.size();
   blts_[0] = new fbBlt_t(
      fbOffs_, // FB ram
      x, y, 
      fb.getWidth(), fb.getHeight(),
      digitStrip_,
      0, 0,
      r_.width_,
      r_.height_
   );
   cmdList.push( blts_[0] );

   cmdList.push( new fbWait_t( WAITFORDRAWINGENGINE, DRAWINGENGINEIDLE ) );
   bltIdx_[1] = cmdList.size();
   blts_[1] = new fbBlt_t(
      fbOffs_, // FB ram
      x, y, 
      fb.getWidth(), fb.getHeight(),
      digitStrip_,
      0, 0,
      r_.width_,
      r_.height_
   );
   cmdList.push( blts_[1] );
}

odomDigit_t::~odomDigit_t( void )
{
}

unsigned long odomDigit_t::advance( unsigned long howMany )
{
   unsigned oldVal = pixelOffs_ ;
   pixelOffs_ += howMany ;
   unsigned rval ;
   
   if( pixelOffs_ > pos9_ )
   {
      if( oldVal < pos9_ )
         oldVal = pos9_ ;

      if( pixelOffs_ < digitStrip_.height() ){
         //
         // need two blts
         //
         rval = pixelOffs_ - oldVal ;

         update();

         return rval ;

      } // partially wrapped, need two blts
      else {
         //
         // this clause fails if howMany approaches or exceeeds digitStrip_.height()
         //

         pixelOffs_ %= digitStrip_.height();

         rval = digitStrip_.height() - oldVal ; // wrapped

         // intentional fall through
      }
   } // wrap (and carry)
   else
      rval = 0 ;

   update();

   return rval ;
}

void odomDigit_t::update()
{
   fbDevice_t &fb = getFB();
   
   if( pixelOffs_ > pos9_ )
   {
      unsigned const firstBltHeight = r_.height_-(pixelOffs_-pos9_);
      blts_[0]->set(
         fbOffs_, // FB ram
         r_.xLeft_, r_.yTop_, 
         fb.getWidth(), fb.getHeight(),
         digitStrip_,
         0, pixelOffs_,
         r_.width_,
         firstBltHeight
      );

      blts_[1]->perform();
      blts_[1]->set(
         fbOffs_, // FB ram
         r_.xLeft_, r_.yTop_ + firstBltHeight, 
         fb.getWidth(), fb.getHeight(),
         digitStrip_,
         0, 0,
         r_.width_,
         r_.height_-firstBltHeight
      );
   } // wrap (and carry)
   else
   {
      // single blt
      blts_[1]->skip();
      blts_[0]->set(
         fbOffs_, // FB ram
         r_.xLeft_, r_.yTop_, 
         fb.getWidth(), fb.getHeight(),
         digitStrip_,
         0, pixelOffs_,
         r_.width_,
         r_.height_
      );
   }
}

void odomDigit_t::show( void )
{
   if( hidden_ )
   {
      hidden_ = false ;
      blts_[0]->perform();
      blts_[1]->perform();
   }
}

void odomDigit_t::hide( void )
{
   if( !hidden_ )
   {
      fbDevice_t &fb = getFB();

      hidden_ = true ;
      blts_[0]->skip();
      blts_[1]->skip();

      // re-draw background
      if( graphicsLayer == mode_ )
         fbBlt( fbOffs_, r_.xLeft_, r_.yTop_, fb.getWidth(), fb.getHeight(),
                background_, 0, 0, r_.width_, r_.height_ );
      else
         sm501alpha_t::get(alphaMode(mode_)).clear( r_ );
   }
}

void odomDigit_t::set( unsigned pos )
{
   pos %= 10 ;
   pixelOffs_ = ( pos * r_.height_ );
   update();
}

#ifdef MODULETEST

#include "imgFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/sm501-int.h>
#include "fbCmdFinish.h"
#include <signal.h>

static bool die = false ;
static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   die = true ;
}

int main( int argc, char const * const argv[] )
{
   fprintf( stderr, "Hello, %s\n", argv[0] );

   if( 2 <= argc ){
      image_t dsImage ;
      if( !imageFromFile( argv[1], dsImage ) ){
         fprintf( stderr, "loading digit strip: %m\n" );
         return -1 ;
      }
   
      odometerMode_e mode = ( ( 2 < argc ) && ( '4' == *argv[2] ) )
                            ? alphaLayer
                            : graphicsLayer ;
      fbImage_t digitImage( dsImage, imageMode(mode) );
      
      fbCommandList_t cmdList ;
   
      odomDigit_t digit( cmdList, digitImage, 0, 0, mode );
   
      cmdList.push( new fbFinish_t );
   
      fbPtr_t cmdListMem( cmdList.size() );
      cmdList.copy( cmdListMem.getPtr() );
   
      fbDevice_t &fb = getFB();
   
      signal( SIGINT, ctrlcHandler );
   
      unsigned advance  = ( 3 < argc ) ? strtoul(argv[3], 0, 0) : 1 ;
      unsigned sleep_ms = ( 4 < argc ) ? strtoul(argv[4], 0, 0 ) : 1 ;
      unsigned initPos  = ( 5 < argc ) ? strtoul(argv[5], 0, 0 ) : 0 ;
   
      digit.set( initPos );
   
      while( !die ){
         unsigned fraction ;
         printf( "%u.%02u: %4u\r", digit.digitValue(fraction), fraction, digit.pixelOffset() );
         unsigned carry = digit.advance(advance);
         if( carry )
            printf( "\n   +%u\n", carry );
         fflush(stdout);
         int rval = ioctl( fb.getFd(), SM501_EXECCMDLIST, cmdListMem.getOffs() );
         if( rval ){
            printf( "result: %d\n", rval );
            break ;
         }
         usleep(sleep_ms*1000);
      }
   
      digit.hide();
   
      cmdList.dump();
   }
   else                     //    0             1        2                 3       4      5
      fprintf( stderr, "Usage: odomDigit digitStripPath mode(565|4444) [advance [sleep [initPos]]]\n" );

   return 0 ;
}

#endif
