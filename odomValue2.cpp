/*
 * Module odomValue2.cpp
 *
 * This module defines the methods of the odomValue2_t
 * class as declared in odomValue2.h
 *
 * Change History : 
 *
 * $Log: odomValue2.cpp,v $
 * Revision 1.4  2002-12-15 08:39:10  ericn
 * -Added width and height parameters, support scaling of images to fit
 *
 * Revision 1.3  2006/10/25 23:26:46  ericn
 * -handle dollar values going down
 *
 * Revision 1.2  2006/10/19 15:28:09  ericn
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
   #define COMMA1BACK   0xF0F0         // green
   #define COMMA2BACK   0xFF00         // red
   #define DECIMALBACK  0xF444         // gray
   #define DOLLARBACK   0xF00F         // blue
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
     unsigned             y,
     unsigned             width,
     unsigned             height )
   : graphics_( graphics )
   , alpha_( sm501alpha_t::get(sm501alpha_t::rgba4444) )
   , fbRam_( alpha_.fbRamOffset() )
   , digitHeight_( graphics.getDecimal(0).height() )
   , xLeft_( x )
   , comma1_( 
        new fbcMoveHide_t( cmdList,
                           fbRam_,
                           x + graphics.comma2Offset(0),
                           y,
                           getFB().getWidth(),
                           getFB().getHeight(),
                           graphics.getComma(0),
                           0, 0, 
                           graphics.getComma(0).width(),
                           graphics.getComma(0).height(),
                           false, COMMA1BACK ) 
   )
   , comma2_( 
        new fbcMoveHide_t( cmdList,
                           fbRam_,
                           x + graphics.comma1Offset(0),
                           y,
                           getFB().getWidth(),
                           getFB().getHeight(),
                           graphics.getComma(0),
                           0, 0, 
                           graphics.getComma(0).width(),
                           graphics.getComma(0).height(),
                           false, COMMA2BACK ) 
   )
   , decimalPoint_(
        new fbcMoveHide_t( cmdList,
                           fbRam_,
                           x + graphics.decimalOffset(0),
                           y,
                           getFB().getWidth(),
                           getFB().getHeight(),
                           graphics.getDecimal(0),
                           0, 0, 
                           graphics.getDecimal(0).width(),
                           graphics.getDecimal(0).height(),
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
   printf( "inside odomValue2_t constructor\n" );
   unsigned const screenWidth = getFB().getWidth();
   unsigned const screenHeight = getFB().getHeight();

   //
   // command list is built based on max significant digits
   // 
   unsigned dig ;
   for( dig = 0 ; dig < maxDigits_ ; dig++ ){
      unsigned offs = graphics.digitOffset(0,maxDigits_-dig-1);
printf( "digit %u at %u\n", dig, offs );
fbImage_t const &image = graphics.getStrip(0);
      digits_[dig] = new fbcCircular_t( cmdList, fbRam_, 
                                        x + offs, y, 
                                        screenWidth, screenHeight,
                                        image,
                                        digitHeight_,
                                        0, false, DIGITBACK(dig) );
   }

   // allocate dollar sign last so it doesn't get cleared by digit clear
   unsigned offs = x+graphics.dollarOffset(0);
   printf( "dollar sign at %u\n", offs );
   dollarSign_ = new fbcMoveHide_t( cmdList,
                                    fbRam_,
                                    x+graphics.dollarOffset(0),
                                    y,
                                    getFB().getWidth(),
                                    getFB().getHeight(),
                                    graphics.getDollar(0),
                                    0, 0, 
                                    graphics.getDollar(0).width(),
                                    graphics.getDollar(0).height(),
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
   else
      comma1_->hide();
   if( msd < comma2Pos )
      comma2_->show();
   else
      comma2_->hide();
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
         fbImage_t const &dollar = graphics_.getDollar(sigDigits_);
         unsigned const xPos = xLeft_ + graphics_.dollarOffset(sigDigits_);
printf( "dollar sign at %u (width %u)\n", xPos, dollar.width() );
         dollarSign_->setX( xPos );
         dollarSign_->swapSource(dollar,0);
         if( sigDigits_ > prevSig_ )
            dollarSign_->skipHide();
         fbImage_t const &digitStrip = graphics_.getStrip(sigDigits_);
         if( graphics_.sigToIndex(prevSig_) != graphics_.sigToIndex(sigDigits_) ){
            for( unsigned i = 0 ; i < maxDigits_ ; i++ ){
               unsigned oldPos = graphics_.digitOffset(prevSig_,maxDigits_-i-1);
               unsigned newPos = graphics_.digitOffset(sigDigits_,maxDigits_-i-1);
               int const diff = newPos - oldPos ;
               if( 0 != diff ){
printf( "move digit %u by %d (%u to %u)\n", i, diff, oldPos, newPos );
                  digits_[i]->setDestX( digits_[i]->getDestX() + diff );
               }
               digits_[i]->swapSource( digitStrip );
            }

            printf( "move decimal point to %u (width %u)\n", xLeft_ + graphics_.decimalOffset(sigDigits_), graphics_.getDecimal(sigDigits_).width() );
            decimalPoint_->setX( xLeft_ + graphics_.decimalOffset(sigDigits_) );
            decimalPoint_->swapSource( graphics_.getDecimal(sigDigits_), 0 );
         }
         if( 5 < sigDigits_ ){
            printf( "move comma2 to %u\n", xLeft_ + graphics_.comma1Offset(sigDigits_) );
            comma2_->setX( xLeft_ + graphics_.comma1Offset(sigDigits_) );
            comma2_->swapSource( graphics_.getComma(sigDigits_), 0 );
            if( 8 < sigDigits_ ){
               printf( "move comma1 to %u\n", xLeft_ + graphics_.comma2Offset(sigDigits_) );
               comma1_->setX( xLeft_ + graphics_.comma2Offset(sigDigits_) );
               comma1_->swapSource( graphics_.getComma(sigDigits_), 0 );
            }
         }
         prevSig_ = sigDigits_ ;
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
#include "rawKbd.h"

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

static unsigned windowWidth = getFB().getWidth();

static void parseArgs( int &argc, char const **argv )
{
   for( int arg = 1 ; arg < argc -1 ; arg++ ){
      char const *param = argv[arg];
      if( '-' == *param ){
         if( 'w' == tolower(param[1]) ){
            windowWidth = strtoul(argv[arg+1],0,0);
            for( int fwd = arg+2 ; fwd < argc ; fwd++ ){
               argv[fwd-2] = argv[fwd];
            }
            argc -= 2 ;
            arg-- ;
         } // width command
      }
   }
}

static void printValue()
{
   printf( "$" );
   for( unsigned i = 0 ; i < odomValue2_t::maxDigits_ ; i++ )
      printf( "%u", value_[i] );
   printf( "\n" );
}

int main( int argc, char const *argv[] )
{
   parseArgs( argc, argv );
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
      
      odomGraphics_t *graphics = new odomGraphics_t( argv[1], alphaLayer, odomValue2_t::maxDigits_, windowWidth );
      if( !graphics->worked() ){
         perror( argv[1] );
         return 1 ;
      }

      printf( "loaded graphics from %s\n", argv[1] );

      if( 2 < argc ){
         convertValue( argv[2], value_ );
      }

      logTraces_t::get(true);

      fbCommandList_t cmdList ;
      printf( "creating value\n" );
      odomValue_ = new odomValue2_t( cmdList, *graphics, 0, 0 );
      cmdList.push( new fbFinish_t );

      printf( "set value\n" );
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
      unsigned digitHeight = graphics->height();

      odomValue_->show();

      rawKbd_t kbd ;
      bool doExit = false ;
      while( !doExit && ( 50000 > (prevTick-startTick) ) ){
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
                     value_[dig] = 9 ;
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
         char c ;
         if( kbd.read( c ) ){
            switch( tolower( c ) ){
               case '+' : {
                  printf( "+++\n" );
                  unsigned i ;
                  for( i = 0 ; i < odomValue2_t::maxDigits_ ; i++ ){
                     if( 0 == value_[i] )
                        ;
                     else
                        break ;
                  }
                  if( 0 < i )
                     value_[i-1] = 9 ;
                  printValue();
                  extra_ = 0 ;
                  odomValue_->set( value_, extra_ );
                  break ;
               }
               case '-' : {
                  printf( "---\n" );
                  unsigned i ;
                  for( i = 0 ; i < odomValue2_t::maxDigits_ ; i++ ){
                     if( 0 == value_[i] )
                        ;
                     else
                        break ;
                  }
                  printf( "msd == %u\n", i );
                  if( odomValue2_t::maxDigits_ > i ){
                     value_[i] = 0 ;
                     if( odomValue2_t::maxDigits_ > i+1 )
                        value_[i+1] = 9 ;
                  }
                  printValue();
                  extra_ = 0 ;
                  odomValue_->set( value_, extra_ );
                  break ;
               }
               case '\x03' :
               case '\x1b' :
                  doExit = true ;
                  break ;
               case '?' :
                  for( unsigned i = 0 ; i < odomValue_->maxDigits_ ; i++ ){
                     printf( "digit %u at %u\n", i, odomValue_->digits_[i]->getDestX() );
                  }
                  printf( "dollar sign at %u, width %u\n", odomValue_->dollarSign_->getX(), odomValue_->dollarSign_->getWidth() );
                  break ;
            }
         }
      }
   }
   else
      fprintf( stderr, "Usage: %s path [path...]\n", argv[0] );

   return 0 ;
}

#endif

