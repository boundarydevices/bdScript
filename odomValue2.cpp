/*
 * Module odomValue2.cpp
 *
 * This module defines the methods of the odomValue2_t
 * class as declared in odomValue2.h
 *
 * Change History : 
 *
 * $Log: odomValue2.cpp,v $
 * Revision 1.2  2006-10-19 15:28:09  ericn
 * -dollar sign at end of list, convenience functions, optional color background
 *
 * Revision 1.1  2006/10/16 22:45:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomValue2.h"

// #define DEBUGPRINT
#include "debugPrint.h"

//
// with 10 digits, we have the following punctuation:
//
//      01 234 567 89
//     $12,345,678.90
//
unsigned const comma1Pos = 2 ;
unsigned const comma2Pos = 5 ;
unsigned const decimalPos = 8 ;

// #define COLORBACKGROUND

#ifdef COLORBACKGROUND
   static unsigned short DIGITBACK( unsigned dig ){
      dig += 4 ; // make range 4..13
      return (unsigned short)( (dig<<12) | (dig<<8) | (dig<<4) | dig );
   }
   #define COMMA1BACK   0xF0F0
   #define COMMA2BACK   0xFF00
   #define DECIMALBACK  0xF444
   #define DOLLARBACK   0xF00F
#else
   #define DIGITBACK(__dig)   0
   
   #define COMMA1BACK   0
   #define COMMA2BACK   0
   #define DECIMALBACK  0
   #define DOLLARBACK   0
#endif

odomValue2_t::odomValue2_t
   ( fbCommandList_t      &cmdList,
     odomGraphics_t const &graphics,
     unsigned             x,
     unsigned             y )
   : graphics_( graphics )
   , alpha_( sm501alpha_t::get(sm501alpha_t::rgba4444) )
   , fbRam_( alpha_.fbRamOffset() )
   , digitHeight_( graphics.decimalPoint_.height() )
   , comma1_( 
        new fbcHideable_t( cmdList,
                           fbRam_,
                           x 
                              + 2*graphics.digitStrip_.width()
                              + graphics.dollarSign_.width(),
                           y,
                           getFB().getWidth(),
                           getFB().getHeight(),
                           graphics.comma_,
                           0, 0, 
                           graphics.comma_.width(),
                           graphics.comma_.height(),
                           false, COMMA1BACK ) 
   )
   , comma2_( 
        new fbcHideable_t( cmdList,
                           fbRam_,
                           x 
                              + 5*graphics.digitStrip_.width()
                              + graphics.dollarSign_.width()
                              + graphics.comma_.width(),
                           y,
                           getFB().getWidth(),
                           getFB().getHeight(),
                           graphics.comma_,
                           0, 0, 
                           graphics.comma_.width(),
                           graphics.comma_.height(),
                           false, COMMA2BACK ) 
   )
   , decimalPoint_(
        new fbcHideable_t( cmdList,
                           fbRam_,
                           x 
                              + 8*graphics.digitStrip_.width()
                              + graphics.dollarSign_.width()
                              + 2*graphics.comma_.width(),
                           y,
                           getFB().getWidth(),
                           getFB().getHeight(),
                           graphics.decimalPoint_,
                           0, 0, 
                           graphics.decimalPoint_.width(),
                           graphics.decimalPoint_.height(),
                           false, DECIMALBACK ) 
   )
   , dollarSign_( 0 )
   , wantHidden_( true )   // app should call show()
   , cmdHidden_( true )    // commands start out hidden
   , isHidden_( false )    // force redraw
   , sigDigits_( 0 )
   , prevSig_( 0 )
   , updating_( false )
{
   unsigned const screenWidth = getFB().getWidth();
   unsigned const screenHeight = getFB().getHeight();

   unsigned const startX = x ;
   x += graphics.dollarSign_.width();
   unsigned dig ;
   for( dig = 0 ; dig < maxDigits_ ; dig++ ){
      if( ( comma1Pos == dig ) || ( comma2Pos == dig ) )
         x += graphics_.comma_.width();
      else if( decimalPos == dig )
         x += graphics_.decimalPoint_.width();
      digits_[dig] = new fbcCircular_t( cmdList, fbRam_, 
                                        x, y, screenWidth, screenHeight,
                                        graphics.digitStrip_,
                                        digitHeight_,
                                        0, false, DIGITBACK(dig) );
      x += graphics.digitStrip_.width();
   }

   // allocate dollar sign last so it doesn't get cleared by digit clear
   dollarSign_ = new fbcMoveHide_t( cmdList,
                                    fbRam_,
                                    startX,
                                    y,
                                    getFB().getWidth(),
                                    getFB().getHeight(),
                                    graphics.dollarSign_,
                                    0, 0, 
                                    graphics.dollarSign_.width(),
                                    graphics.dollarSign_.height(),
                                    false, DOLLARBACK );
}


odomValue2_t::~odomValue2_t( void )
{
}


void odomValue2_t::set
   ( unsigned char digits[maxDigits_],
     unsigned      extraPixels )
{
   updating_ = true ;
   // walk from right to left
   unsigned msd = 0 ;
   for( unsigned dig = maxDigits_ ; 0 < dig ; ){
      --dig ;
      unsigned char value = digits[dig];
      digitValues_[dig] = value ;
      if( 0 != value )
         msd = maxDigits_-dig ;
      extraPixels = digits_[dig]->setOffset( value*digitHeight_ + extraPixels );
      if( 9 > value )
         extraPixels = 0 ;
   }

   // always show at least three digits
   if( msd > 3 )
      sigDigits_ = msd ;
   else
      sigDigits_ = 3 ;

   updating_ = false ;
}


void odomValue2_t::executed()
{
   isHidden_ = cmdHidden_ ;
   comma1_->executed();
   comma2_->executed();
   decimalPoint_->executed();
   for( unsigned i = 0 ; i < maxDigits_ ; i++ )
      digits_[i]->executed();
   dollarSign_->executed();
}

void odomValue2_t::hideEm( void )
{
debugPrint( "hide value\n" );

   dollarSign_->hide();
   comma1_->hide();
   comma2_->hide();
   decimalPoint_->hide();
   for( unsigned i = 0 ; i < maxDigits_ ; i++ )
      digits_[i]->hide();
}

void odomValue2_t::showEm( unsigned msd )
{
debugPrint( "show value\n" );

   dollarSign_->show();
   if( msd < comma1Pos )
      comma1_->show();
   if( msd < comma2Pos )
      comma2_->show();
   decimalPoint_->show();
   unsigned i ;
   for( i = 0 ; i < msd ; i++ ){
      digits_[i]->hide();
   }
   for( ; i < maxDigits_ ; i++ ){
      digits_[i]->show();
   }
}

void odomValue2_t::updateCommandList()
{
   if( !updating_ ){
      unsigned msd = maxDigits_-sigDigits_ ;
      bool changed = false ;
      if( sigDigits_ != prevSig_ ){
         changed = true ;
debugPrint( "%u significant digits, msd = %u, want %d, cmd %d\n", sigDigits_, msd, wantHidden_, cmdHidden_ );
         prevSig_ = sigDigits_ ;
         dollarSign_->setX( digits_[msd]->getDestX() - dollarSign_->getWidth() );
      }

      if( wantHidden_ != cmdHidden_ ){
debugPrint( "hidden %d->%d\n", cmdHidden_, wantHidden_ );
         cmdHidden_ = wantHidden_ ;
         changed = true ;
      }

      if( changed ){
         if( wantHidden_ ){
            hideEm();
         }
         else {
            showEm( msd );
         }
      }
      comma1_->updateCommandList();
      comma2_->updateCommandList();
      decimalPoint_->updateCommandList();
      for( unsigned i = 0 ; i < maxDigits_ ; i++ )
         digits_[i]->updateCommandList();
      dollarSign_->updateCommandList();
   }
   else
      debugPrint( "updating...\n" );
}

void odomValue2_t::show()
{
   updating_ = true ;
   wantHidden_ = false ;
   updating_ = false ;
}

void odomValue2_t::hide()
{
   updating_ = true ;
   wantHidden_ = true ;
   updating_ = false ;
}

void odomValue2_t::dump( void )
{
   printf( "dollarSign:\n" ); dollarSign_->dump();
   printf( "comma1:\n" ); comma1_->dump();
   printf( "comma2:\n" ); comma2_->dump();

   printf( "decimal:\n" ); decimalPoint_->dump();

   for( unsigned i = 0 ; i < maxDigits_ ; i++ ){
      printf( "digit[%u]\n", i );
      digits_[i]->dump();
   }
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
#include "blockSig.h"

#define LOGTRACES
#include "logTraces.h"

static traceSource_t traceVsync( "vsync" );
static traceSource_t traceCmdList( "cmdList" );
static traceSource_t traceUpdate( "update" );

static fbCmdListSignal_t *cmdListDev_ = 0 ;
static unsigned volatile vsyncCount = 0 ;
static unsigned volatile cmdComplete = 0 ;

static odomValue2_t *odomValue_ = 0 ;

static unsigned char value_[odomValue2_t::maxDigits_] = {
   0, 0, 0, 0, 0,
   9, 9, 9, 8, 8
};

static unsigned extra_ = 0 ;
static void videoOutput( int signo, void *param )
{
   TRACE_T( traceVsync, trace );
   fbPtr_t *mem = (fbPtr_t *)param ;

   if( cmdListDev_ && mem ){
      ++vsyncCount ;
      unsigned offset = mem->getOffs();
      int numWritten = write( cmdListDev_->getFd(), &offset, sizeof( offset ) );
      if( numWritten != sizeof( offset ) ){
         perror( "write(cmdListDev_)" );
         exit(1);
      }
   }
}

static void cmdListComplete( int signo, void *param )
{
   TRACE_T( traceCmdList, trace );
   ++cmdComplete ;
   odomValue_->executed();
   odomValue_->updateCommandList();
}

static void convertValue( char const *s, unsigned char value[] ){
   unsigned digitPos = odomValue2_t::maxDigits_ ;
   unsigned i = strlen(s);
   while( ( 0 < i ) && ( 0 < digitPos ) ){
      --i ;
      unsigned char dig = s[i] - '0' ;
      if( 9 >= dig ){
         value[--digitPos] = dig ;
      }
      else {
         printf( "Invalid digit %c\n", s[i] );
         break ;
      }
   }
}

int main( int argc, char const * const argv[] )
{
   if( 1 < argc )
   {
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
      
      odomGraphics_t *graphics = new odomGraphics_t( argv[1], alphaLayer );
      if( !graphics->worked() ){
         perror( argv[1] );
         return 1 ;
      }

      if( 2 < argc ){
         convertValue( argv[2], value_ );
      }

      logTraces_t::get(true);

      fbCommandList_t cmdList ;
      odomValue_ = new odomValue2_t( cmdList, *graphics, 0, 0 );
      cmdList.push( new fbFinish_t );

      odomValue_->set( value_, extra_ );

      fbPtr_t cmdListMem( cmdList.size() );
      cmdList.copy( cmdListMem.getPtr() );
      hexDumper_t dump( cmdListMem.getPtr(), cmdListMem.size(), cmdListMem.getOffs() );
      while( dump.nextLine() )
         printf( "%s\n", dump.getLine() );

      cmdListDev_ = &cmdListDev ;

      sigaddset( &blockThese, cmdListDev.getSignal() );
      sigaddset( &blockThese, vsync.getSignal() );

      setSignalHandler( vsync.getSignal(), blockThese, videoOutput, &cmdListMem );
      setSignalHandler( cmdListDev.getSignal(), blockThese, cmdListComplete, 0 );

      vsync.enable();
      cmdListDev.enable();

      printf( "executed\n" );
      long long const startTick = tickMs();
      long long prevTick = startTick ;

      unsigned prevSync = vsyncCount ;
      unsigned digitHeight = graphics->comma_.height();

      odomValue_->show();

      while( 50000 > (prevTick-startTick) ){
         pause();
         if( cmdComplete == vsyncCount ){
            unsigned ticks = (vsyncCount-prevSync)*3;
            prevSync = vsyncCount ;
            unsigned carry = 0 ;
            extra_ += ticks ;
   
            for( unsigned dig = odomValue2_t::maxDigits_ ; ( 0 < dig ); ){
               --dig ;
               value_[dig] += carry ;
               if( carry )
                  printf( "carry a %u\n", carry );
               carry = 0 ;
               while( extra_ >= digitHeight ){
                  extra_ -= digitHeight ;
                  value_[dig]++ ;
                  if( 9 < value_[dig] ){
                     value_[dig] = 0 ;
                     carry++ ;
                  }
               }
               if( 9 < value_[dig] ){
                  value_[dig] = 0 ;
                  carry++ ;
               }
            }
   
            { TRACE_T( traceUpdate, trace );
              blockSignal_t block( cmdListDev.getSignal() );
              odomValue_->set( value_, extra_ );
            }
         }
      }
   }
   else
      fprintf( stderr, "Usage: %s imgFile [imgFile...]\n", argv[0] );

   return 0 ;
}

#endif

