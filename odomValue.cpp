/*
 * Module odomValue.cpp
 *
 * This module defines the methods of the odomValue_t
 * class as declared in odomValue.h
 *
 *
 * Change History : 
 *
 * $Log: odomValue.cpp,v $
 * Revision 1.2  2006-09-23 15:27:06  ericn
 * -cleanup debug stuff
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomValue.h"
#include <stdio.h>
#include <unistd.h>
#include "tickMs.h"

static unsigned const maxDigits_ = 8 ;


odomValue_t::odomValue_t
   ( fbCommandList_t      &cmdList,
     odomGraphics_t const &graphics,
     unsigned              x,
     unsigned              y,
     odometerMode_e        mode )
   : graphics_(graphics)
   , alpha_( sm501alpha_t::get( alphaMode(mode) ) )
   , mode_( mode )
   , commaPos_(0)
   , background_( x, y, 
                  maxDigits_*graphics.digitStrip_.width()
                  + graphics.comma_.width()
                  + graphics.decimalPoint_.width()
                  + graphics.dollarSign_.width(),
                  graphics.dollarSign_.height() )
   , sigDigits_( 0 )
   , pennies_( 0 )
{
   r_.xLeft_  = x ;
   r_.yTop_   = y ;
   r_.width_  = background_.width();
   r_.height_ = background_.height();

   unsigned const commaIdx = maxDigits_-2-3 ;
   unsigned const decimalIdx = maxDigits_-2 ;

   x += graphics.dollarSign_.width();
   unsigned d ;
   for( d = 0 ; d < commaIdx ; d++ )
   {
      digits_[d] = new odomDigit_t( 
         cmdList,
         graphics.digitStrip_,
         x, y, mode );
      x += graphics.digitStrip_.width();
   }

   commaRect_ = makeRect(x, r_.yTop_, graphics_.comma_.width(), graphics_.comma_.height());
   commaPos_ = x ;
   
   x += graphics.comma_.width();
   for( ; d < decimalIdx ; d++ )
   {
      digits_[d] = new odomDigit_t( 
         cmdList,
         graphics.digitStrip_,
         x, y, mode );
      x += graphics.digitStrip_.width();
   }

   fbDevice_t &fb = getFB();

   decimalRect_ = makeRect( x, r_.yTop_, graphics.decimalPoint_.width(), graphics.decimalPoint_.height() );
   if( graphicsLayer == mode_ ){
      alpha_.set( decimalRect_,
                  (unsigned char const *)graphics_.decimalAlpha_.getPtr());
      fbBlt( fb.getRamOffs(), x, r_.yTop_, fb.getWidth(), fb.getHeight(),
             graphics.decimalPoint_, 0, 0, graphics.decimalPoint_.width(), graphics.decimalPoint_.height() );
   } // graphics layer
   else {
      alpha_.draw4444( (unsigned short *)graphics_.decimalPoint_.pixels(),
                       decimalRect_.xLeft_, r_.yTop_,
                       graphics_.decimalPoint_.width(),
                       graphics_.decimalPoint_.height() );
   } // alpha layer

   x += graphics.decimalPoint_.width();
   for( ; d < maxDigits_ ; d++ )
   {
      digits_[d] = new odomDigit_t( 
         cmdList,
         graphics.digitStrip_,
         x, y, mode );
      x += graphics.digitStrip_.width();
   }
   dollarRect_ = makeRect(r_.xLeft_, r_.yTop_, graphics_.dollarSign_.width(), graphics_.dollarSign_.height());
}

odomValue_t::~odomValue_t( void )
{
   fbDevice_t &fb = getFB();
   fbBlt( fb.getRamOffs(), r_.xLeft_, r_.yTop_, fb.getWidth(), fb.getHeight(),
          background_, 0, 0, background_.width(), background_.height() );
}

void odomValue_t::drawDigit( unsigned idx )
{
   odomDigit_t *dig = digits_[maxDigits_-1-idx];
   if( dig->isHidden() && ( graphicsLayer == mode_ ) )
   {
      alpha_.set( dig->getRect(), 
                  (unsigned char const *)graphics_.digitAlpha_.getPtr());
   }
   dig->show();
}

void odomValue_t::hideDigit( unsigned idx )
{
   odomDigit_t *dig = digits_[maxDigits_-1-idx];
   if( dig->isVisible() ){
      dig->hide();
      if( graphicsLayer == mode_ ) 
         alpha_.clear(dig->getRect());
   }
}

// initialize to a particular value 
void odomValue_t::set( unsigned pennies )
{
   pennies_ = pennies ;

   unsigned d ;
   for( d = 0 ; ( d < maxDigits_ ) && (pennies || (3 > d)); d++ )
   {
      odomDigit_t *dig = digits_[maxDigits_-1-d];
      dig->set( pennies % 10 );
      pennies /= 10 ;
      if( graphicsLayer == mode_ )
         alpha_.set( dig->getRect(), 
                     (unsigned char const *)graphics_.digitAlpha_.getPtr());
      dig->show();
   }

   sigDigits_ = d ;
   
   fbDevice_t &fb = getFB();
   
   if( sigDigits_ > 5 ){
      if( graphicsLayer == mode_ ){
         alpha_.set( commaRect_, 
                     (unsigned char const *)graphics_.commaAlpha_.getPtr());
         fbBlt( fb.getRamOffs(), commaPos_, r_.yTop_, fb.getWidth(), fb.getHeight(),
                graphics_.comma_, 0, 0, graphics_.comma_.width(), graphics_.comma_.height() );
      } // graphics layer
      else {
         alpha_.draw4444( (unsigned short *)graphics_.comma_.pixels(),
                          commaPos_, r_.yTop_,
                          graphics_.comma_.width(),
                          graphics_.comma_.height() );
      } // alpha layer
   }

   for( ; d < maxDigits_ ; d++ )
   {
      odomDigit_t *dig = digits_[maxDigits_-1-d];
      dig->hide();
   }
   
   dollarRect_.xLeft_ = digits_[maxDigits_-sigDigits_]->getX() - graphics_.dollarSign_.width();
   if( graphicsLayer == mode_ ) {
      alpha_.set( dollarRect_, (unsigned char const *)graphics_.dollarAlpha_.getPtr());
      fbBlt( fb.getRamOffs(), 
             dollarRect_.xLeft_, r_.yTop_, fb.getWidth(), fb.getHeight(),
             graphics_.dollarSign_, 0, 0, graphics_.dollarSign_.width(), graphics_.dollarSign_.height() );
   } else {
      alpha_.draw4444( (unsigned short *)graphics_.dollarSign_.pixels(),
                       dollarRect_.xLeft_, r_.yTop_, 
                       graphics_.dollarSign_.width(),
                       graphics_.dollarSign_.height() );
   } // alpha layer
}

// advance by specified number of pixels
void odomValue_t::advance( unsigned numPixels )
{
   unsigned pennies = 0 ;
   unsigned mult = 1 ;

   unsigned d ;
   for( d = 0 ; d < maxDigits_ ; d++ )
   {
      odomDigit_t *dig = digits_[maxDigits_-1-d];

      unsigned fraction ;
      pennies += dig->digitValue(fraction) * mult ;
      mult *= 10 ;

      unsigned carry = dig->advance(numPixels);
      
      drawDigit(d);
      
      if( 0 == carry )
         break ;
      else
         numPixels = carry ;
   }

   d++ ;

   if( d > sigDigits_ ){
      fbDevice_t &fb = getFB();

      sigDigits_ = d ;
      if( sigDigits_ > 5 ){
         if( graphicsLayer == mode_ ){
            alpha_.set( makeRect(commaPos_, r_.yTop_, graphics_.comma_.width(), graphics_.comma_.height()), 
                        (unsigned char const *)graphics_.commaAlpha_.getPtr());
            fbBlt( fb.getRamOffs(), commaPos_, r_.yTop_, fb.getWidth(), fb.getHeight(),
                   graphics_.comma_, 0, 0, graphics_.comma_.width(), graphics_.comma_.height() );
         } // graphics layer
         else {
            alpha_.draw4444( (unsigned short *)graphics_.comma_.pixels(),
                             commaPos_, r_.yTop_,
                             graphics_.comma_.width(),
                             graphics_.comma_.height() );
         } // alpha layer
      }
      else if( graphicsLayer == mode_ ){
         alpha_.clear( makeRect(commaPos_, r_.yTop_, graphics_.comma_.width(), graphics_.comma_.height()) );
      }

      dollarRect_.xLeft_ = digits_[maxDigits_-d]->getX() - graphics_.dollarSign_.width();
      if( graphicsLayer == mode_ ) {
         alpha_.set( dollarRect_, (unsigned char const *)graphics_.dollarAlpha_.getPtr());
         fbBlt( fb.getRamOffs(), 
                dollarRect_.xLeft_, r_.yTop_, fb.getWidth(), fb.getHeight(),
                graphics_.dollarSign_, 0, 0, graphics_.dollarSign_.width(), graphics_.dollarSign_.height() );
      } else {
         alpha_.draw4444( (unsigned short *)graphics_.dollarSign_.pixels(),
                          dollarRect_.xLeft_, r_.yTop_, 
                          graphics_.dollarSign_.width(),
                          graphics_.dollarSign_.height() );
      }
   }
   
   // calculate entire value
   for( ; d < maxDigits_ ; d++ )
   {
      odomDigit_t *dig = digits_[maxDigits_-1-d];
      unsigned fraction ;
      pennies += dig->digitValue(fraction) * mult ;
      mult *= 10 ;
   }

   pennies_ = pennies ;
}

#ifdef MODULETEST

#include <stdio.h>
#include "imgFile.h"
#include <sys/ioctl.h>
#include <linux/sm501-int.h>
#include "fbCmdFinish.h"
#include "odomGraphics.h"
#include <signal.h>
#include "fbDev.h"
#include "tickMs.h"
#include <fcntl.h>
#include "rtSignal.h"

static bool volatile die = false ;
static bool volatile die2 = false ;

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   die = true ;
}

class odomInfo_t {
public: 
   odomInfo_t( odomGraphics_t const &graphics,
               unsigned              initValue,
               unsigned              x,
               unsigned              y,
               odometerMode_e        mode );
   ~odomInfo_t( void );

   fbCommandList_t cmdList_ ;
   fbPtr_t         cmdListMem_ ;
   odomValue_t     value_ ;
private:
   odomInfo_t( odomInfo_t const & );
};

odomInfo_t::odomInfo_t(
   odomGraphics_t const &graphics,
   unsigned              initValue,
   unsigned              x,
   unsigned              y,
   odometerMode_e        mode )
   : cmdList_()
   , cmdListMem_()
   , value_( cmdList_, graphics, x, y, mode )
{
   cmdList_.push( new fbFinish_t );

   cmdListMem_ = fbPtr_t( cmdList_.size() );
   cmdList_.copy( cmdListMem_.getPtr() );

   value_.set( initValue );
}

static unsigned long issueCount = 0 ;
static unsigned long completionCount = 0 ;

static unsigned advance = 1 ;
static unsigned numOdoms = 0 ;
static odomInfo_t ** odometers ;
static unsigned long * cmdList ;
static FILE *fOut = 0 ;

void dumpCmdList( void )
{
   printf( "--> cmdList:\n" );
   for( unsigned i = 0 ; i < numOdoms ; i++ ){
      printf( "[%u] -- 0x%08lx, length 0x%x\n", i, cmdList[i], odometers[i]->cmdList_.size() );
      unsigned const numLongs = odometers[i]->cmdList_.size()/4 ;
      unsigned long const *longs = (unsigned long *)odometers[i]->cmdListMem_.getPtr();
      for( unsigned l = 0 ; l < numLongs ; l++ ){
         printf( "%08lx  ", *longs++ );
         if( 7 == (l & 7) )
            printf( "\n" );
      }
      printf( "\n" );
   }
}

static void writeCmdList( void )
{
   int numWritten = fwrite( cmdList, sizeof(cmdList[0]), numOdoms, fOut );
   fflush(fOut);
   if( numOdoms != (unsigned)numWritten ){
      perror( "write cmdlist" );
      die = true ;
   }
   else
      ++issueCount;
}

static void cmdListHandler( int signo, siginfo_t *info, void *context )
{
   if( !die ){
      for( unsigned i = 0 ; i < numOdoms ; i++ )
         odometers[i]->value_.advance(advance);
      ++completionCount ;
   }
   else
      die2 = true ;
}

static void sigioHandler( int signo, siginfo_t *info, void *context )
{
   printf( "SIGIO!\n" );
   die = true ;
}

static void vsyncHandler( int signo, siginfo_t *info, void *context )
{
   if( !die && ( completionCount == issueCount ) )
      writeCmdList();
}

int main( int argc, char const * const argv[] )
{
   printf( "Hello, %s\n", argv[0] );

   if( 1 < argc ){
      odometerMode_e mode = ( ( 2 < argc ) && ( '4' == *argv[2] ) )
                            ? alphaLayer
                            : graphicsLayer ;

      char const *graphicsDir = argv[1];
      odomGraphics_t const graphics( graphicsDir, mode );
      if( !graphics.worked() ){
         fprintf( stderr, "%s: %m\n", graphicsDir );
         return -1 ;
      }

      fbDevice_t &fb = getFB();
   
      unsigned const digitHeight = graphics.decimalPoint_.height();
      numOdoms = fb.getHeight()/digitHeight ;
numOdoms = 1 ;
      printf( "%u values will fit on the screen: digitHeight == %u\n", numOdoms, digitHeight );
   
      advance = ( 3 < argc ) ? strtoul(argv[3], 0, 0) : 1 ;
      unsigned const initVal  = ( 4 < argc ) ? strtoul(argv[4], 0, 0 ) : 0 ;
   
      odometers = new odomInfo_t *[numOdoms];
      cmdList = new unsigned long [numOdoms];
      unsigned y = 0 ;
      for( unsigned i = 0 ; i < numOdoms ; i++ )
      {
         printf( "odom[%u] at row %u\n", i, y );
         odometers[i] = new odomInfo_t( graphics, initVal, 0, y, mode );
         if( 0 == i )
            printf( "w: %u, h:%u\n"
                    "backSize: %u\n", 
                    odometers[i]->value_.getRect().width_,
                    odometers[i]->value_.getRect().height_,
                    odometers[i]->value_.getBackground().memSize() );
         printf( "done[%u]\n", i );
         y += digitHeight ;
         cmdList[i] = odometers[i]->cmdListMem_.getOffs();
      }
   
      fOut = fopen( "/dev/sm501cmdlist", "wb" );
      if( !fOut ){
         perror( "/dev/sm501cmdlist" );
         return -1 ;
      }
      
      int const fdSync = open( "/dev/sm501vsync", O_RDONLY );
      if( 0 > fdSync ){
         perror( "/dev/sm501vsync" );
         return -1 ;
      }
   
      int const cmdListSignal = nextRtSignal();
      int const vsyncSignal = nextRtSignal();
   
      sigset_t signals ;
      sigemptyset( &signals );
      sigaddset( &signals, cmdListSignal );
   
      struct sigaction sa ;
      sa.sa_flags = SA_SIGINFO|SA_RESTART ;
      sa.sa_restorer = 0 ;
      sigemptyset( &sa.sa_mask );
   
      fcntl(fileno(fOut), F_SETOWN, getpid());
      fcntl(fileno(fOut), F_SETSIG, cmdListSignal );
      sa.sa_sigaction = cmdListHandler ;
      sigaction(cmdListSignal, &sa, 0 );
   
      fcntl(fdSync, F_SETOWN, getpid());
      fcntl(fdSync, F_SETSIG, vsyncSignal );
      sa.sa_sigaction = vsyncHandler ;
      sigaction(vsyncSignal, &sa, 0 );
   
      sa.sa_sigaction = sigioHandler ;
      sigaction(SIGIO, &sa, 0 );
   
      int flags = fcntl( fileno(fOut), F_GETFL, 0 );
      fcntl( fileno(fOut), F_SETFL, flags | O_NONBLOCK | FASYNC );
      flags = fcntl( fdSync, F_GETFL, 0 );
      fcntl( fdSync, F_SETFL, flags | O_NONBLOCK | FASYNC );
   
      signal( SIGINT, ctrlcHandler );
   
      long long start = tickMs();
   
      writeCmdList();
   //   dumpCmdList();
   
      while( !die )
      {
         sleep( 1 );
      }
      long long end = tickMs();
   
      close( fdSync );
   
      while( !die2 && ( issueCount != completionCount ) ){
         printf( "waiting...\n" );
         usleep(1000);
      }
      
      printf( "%lu/%lu iterations in %lu ms\n", issueCount, completionCount, (unsigned long)(end-start) );
      for( unsigned i = 0 ; i < numOdoms ; i++ )
      {
         unsigned pennies = odometers[i]->value_.value();
         unsigned const dollars = pennies / 100 ;
         pennies %= 100 ;
      
         printf( "value[%u] == %u.%02u\n", i, dollars, pennies );
      }
      
   /*
      unsigned char const *dalpha = (unsigned char *)graphics.decimalAlpha_.getPtr();
      for( unsigned y = 0 ; y < graphics.decimalPoint_.height() ; y++ ){
         printf( "%u: ", y );
         for( unsigned x = 0 ; x < graphics.decimalPoint_.width() ; x++ ){
            printf( "%02x ", *dalpha++ );
         }
         printf( "\n" );
      }
   
      char inBuf[256];
      printf( "hit <Enter> to continue\n" );
      fgets( inBuf,sizeof(inBuf),stdin);
   
   */
   }
   else                 //         0         1           2           3          4
      fprintf( stderr, "Usage: odomValue graphicsDir [mode=565] [advance=1] [initVal=0]\n" );
   
   return 0 ;
}

#endif
