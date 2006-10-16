/*
 * Module multiSignal.cpp
 *
 * This module defines the set and clear signal
 * handler routines and the internals needed to 
 * make multiplexed signals work.
 *
 *
 * Change History : 
 *
 * $Log: multiSignal.cpp,v $
 * Revision 1.3  2006-10-16 22:29:01  ericn
 * removed setSignalMask() routine, added dumpSignalHandlers()
 * moved blockSignal_t into blockSignal.h/.cpp
 *
 * Revision 1.2  2006/10/10 20:48:38  ericn
 * -fix mask logic, add setSignalMask()
 *
 * Revision 1.1  2006/09/23 15:28:14  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "multiSignal.h"
#include <assert.h>
#include <stdio.h>
#include "rtSignal.h"
#include <string.h>
#include "blockSig.h"

// #define DEBUGPRINT
#include "debugPrint.h"

#define MAXMULTIPLEXED_SIGNALS   16
#define MAXHANDLERS_PER_SIGNAL   16

typedef struct {
   signalHandler_t   handler_ ;
   void             *handlerParam_ ;
} signalData_t ;

static signalData_t *handlers_[MAXMULTIPLEXED_SIGNALS] = {0};
static bool sigioInstalled = false ;

static void rootHandler( int signo, siginfo_t *info, void *context )
{
   assert( SIGRTMIN <= signo );
   assert( SIGRTMIN + MAXMULTIPLEXED_SIGNALS > signo );

   debugPrint( "rootHandler for signal %d\n", signo );
   unsigned const sigoffs = signo-SIGRTMIN;
   signalData_t const *handlers = handlers_[sigoffs];
   while( handlers->handler_ ){
      handlers->handler_( signo, handlers->handlerParam_ );
      handlers++ ;
   }
}

static void sigioHandler( int signo, siginfo_t *info, void *context )
{
   for( int i = minRtSignal() ; i < maxRtSignal(); i++ ){
      blockSignal_t block( i );
      signalData_t *handlers = handlers_[i];
      if( handlers ){
         for( int j = 0 ; j < MAXHANDLERS_PER_SIGNAL ; j++ ){
            signalData_t &data = handlers[j];
            if( data.handler_ ){
               data.handler_( i, data.handlerParam_ );
            }
            else
               break ;
         } // look for our spot
      }
   }
}

void setSignalHandler
   ( int              signo, 
     sigset_t        &mask,    // mask these signals. Reset on e
     signalHandler_t  handler,
     void            *handlerContext )
{
   debugPrint( "set handler %p+%p for signal %d\n", handler, handlerContext, signo );
   if( SIGRTMIN > signo ){
      fprintf( stderr, "Invalid signal %d/%d\n", signo, SIGRTMIN );
   }
   assert( SIGRTMIN <= signo );
   assert( SIGRTMIN + MAXMULTIPLEXED_SIGNALS > signo );
   assert( 0 != handler );

   unsigned const sigoffs = signo-SIGRTMIN ;

   if( 0 == handlers_[sigoffs] ){
      signalData_t *const handlers = new signalData_t [MAXHANDLERS_PER_SIGNAL];
      memset( handlers, 0, MAXHANDLERS_PER_SIGNAL*sizeof( *handlers ) );
      handlers_[sigoffs] = handlers ;
   }

   blockSignal_t block( signo );
   signalData_t *const handlers = handlers_[sigoffs];

   unsigned i ;
   for( i = 0 ; i < MAXHANDLERS_PER_SIGNAL ; i++ ){
      signalData_t &data = handlers[i];
      if( 0 == data.handler_ ){
         data.handler_ = handler ;
         data.handlerParam_ = handlerContext ;
         break ;
      }
   }

   assert( i < MAXHANDLERS_PER_SIGNAL );
   
   if( 0 == i ){
      struct sigaction sa ;
      sa.sa_flags = SA_SIGINFO|SA_RESTART ;
      sa.sa_restorer = 0 ;
      sa.sa_mask = mask ;
      sa.sa_sigaction = rootHandler ;
      sigaction(signo, &sa, 0 );
   } // first one, set signal handler

   if( !sigioInstalled ){
      sigioInstalled = 1 ;
      struct sigaction sa ;
      sa.sa_flags = SA_SIGINFO|SA_RESTART ;
      sa.sa_restorer = 0 ;
      sigemptyset( &sa.sa_mask );
      sa.sa_sigaction = sigioHandler ;
      sigaction(SIGIO, &sa, 0 );
   }
}

void clearSignalHandler
   ( int             signo,
     signalHandler_t handler,
     void           *handlerContext )
{
   if( SIGRTMIN > signo ){
      fprintf( stderr, "Invalid signal %d/%d\n", signo, SIGRTMIN );
   }
   assert( SIGRTMIN <= signo );
   assert( SIGRTMIN + MAXMULTIPLEXED_SIGNALS > signo );
   assert( 0 != handler );
   
   signalData_t *handlers = handlers_[signo-SIGRTMIN];
   assert( 0 != handlers ); // something should be allocated

   blockSignal_t block( signo );
   while( ( 0 != handlers->handler_ ) 
          &&
          ( handler != handlers->handler_ ) ){
      handlers++ ;
   }

   unsigned i ;
   for( i = 0 ; i < MAXHANDLERS_PER_SIGNAL ; i++ ){
      signalData_t &data = handlers[i];
      assert( 0 != data.handler_ );

      if( ( handler == data.handler_ )
          &&
          ( handlerContext == data.handlerParam_ ) )
      {
         break ; // found handler
      }
   } // look for our spot

   assert( i < MAXHANDLERS_PER_SIGNAL );

   //
   // Shift any remaining handlers up in the list
   //
   unsigned j = 0 ; 
   for( j = i+1 ; j < MAXHANDLERS_PER_SIGNAL ; j++, i++ ){
      if( handlers[j].handler_ ){
         handlers[i] = handlers[j];
      }
      else
         break ;
   }
   handlers[i].handler_ = 0 ;
   handlers[i].handlerParam_ = 0 ;

   if( 0 == i ){
      signal( signo, SIG_IGN );
   } // block signal
}

void dumpSignalHandlers( int signo )
{
   if( SIGRTMIN > signo ){
      fprintf( stderr, "%s: Invalid signal %d/%d\n", __PRETTY_FUNCTION__, signo, SIGRTMIN );
      return ;
   }
   assert( SIGRTMIN <= signo );
   assert( SIGRTMIN + MAXMULTIPLEXED_SIGNALS > signo );
   unsigned const sigoffs = signo-SIGRTMIN ;

   if( 0 == handlers_[sigoffs] ){
      printf( "%s: No handlers\n", __PRETTY_FUNCTION__ );
      return ;
   }

   signalData_t *handlers = handlers_[sigoffs];
   printf( "signal handlers for signal %d/%p\n", signo, handlers );
   unsigned i ;
   for( i = 0 ; i < MAXHANDLERS_PER_SIGNAL ; i++ ){
      signalData_t &data = handlers[i];
      if( 0 != data.handler_ ){
         printf( "   handler %p, param %p\n", data.handler_, data.handlerParam_ );
      }
      else
         break ;
   } // look for our spot

   printf( "--- masked signals: \n" );
   sigset_t blocked ;
   int rc = pthread_sigmask( 0, 0, &blocked );
   assert( 0 == rc );
   for( int sig = minRtSignal() ; sig <= maxRtSignal(); sig++ ){
      if( sigismember( &blocked, sig ) )
         printf( "   %u\n", sig );
   }
}

#ifdef MODULETEST
#include <stdio.h>
#include "rtSignal.h"
#include "fbDev.h"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

static int volatile numDots = 0 ;

class walkingDot_t {
public:
   walkingDot_t( int           signo,
                 fbDevice_t   &fb,
                 int           fdSync,
                 unsigned      ticksPerMove,
                 unsigned      x,
                 unsigned      y,
                 unsigned      xIncr,
                 unsigned      yIncr,
                 unsigned long rgb );
   ~walkingDot_t( void );

   void tick(void);

private:
   int      const signo_ ;
   fbDevice_t          &fb_ ;
   int      const       fdSync_ ;
   unsigned const       ticksPerMove_ ;
   unsigned const       xIncr_ ;
   unsigned const       yIncr_ ;
   unsigned short const rgb_ ;
   unsigned             xPos_ ;
   unsigned             yPos_ ;
   unsigned             ticksSinceLast_ ;
   unsigned short       prevRGB_ ;
};

void wdHandler( int signo, void *param)
{
   walkingDot_t *wd = (walkingDot_t *)param ;
   wd->tick();
}

walkingDot_t::walkingDot_t
   ( int           signo,
     fbDevice_t   &fb,
     int           fdSync,
     unsigned      ticksPerMove,
     unsigned      x,
     unsigned      y,
     unsigned      xIncr,
     unsigned      yIncr,
     unsigned long rgb )
   : signo_( signo )
   , fb_( fb )
   , fdSync_( fdSync )
   , ticksPerMove_( ticksPerMove )
   , xIncr_( xIncr )
   , yIncr_( yIncr )
   , rgb_( fb.get16(rgb) )
   , xPos_( x )
   , yPos_( y )
   , ticksSinceLast_( 0 )
   , prevRGB_( 0 )
{
   ++numDots ;
   sigset_t block ;
   sigemptyset( &block );
   setSignalHandler( signo, block, wdHandler, this );
   if( ( xPos_ < fb.getWidth() )
       &&
       ( yPos_ < fb.getHeight() ) )
   {
      prevRGB_ = fb.getPixel(xPos_, yPos_ );
   }
}

walkingDot_t::~walkingDot_t( void )
{   
   --numDots ;
}

void walkingDot_t::tick(void)
{
   if( ++ticksSinceLast_ >= ticksPerMove_ ){
      unsigned prevX = xPos_ ;
      unsigned prevY = yPos_ ;
      fb_.setPixel(prevX, prevY, prevRGB_);

      xPos_ += xIncr_ ;
      yPos_ += yIncr_ ;
      if( ( xPos_ < fb_.getWidth() )
          &&
          ( yPos_ < fb_.getHeight() ) ){
         fb_.setPixel(prevX, prevY, prevRGB_);
         prevRGB_ = fb_.getPixel(xPos_,yPos_);
         fb_.setPixel(xPos_,yPos_, rgb_ );
         ticksSinceLast_ = 0 ;
      }
      else {
         clearSignalHandler( signo_, wdHandler, this );
         delete this ;
      }
   }
}

int main( int argc, char const * const argv[] ){
   printf( "Hello, %s\n", argv[0] );

   if( 1 < argc ){
      unsigned count = strtoul( argv[1], 0, 0 );
      if( 0 < count ){
         unsigned tpm   = ( 2 < argc ) ? strtoul(argv[2],0,0) : 1 ;
         unsigned xincr = ( 3 < argc ) ? strtoul(argv[3],0,0) : 1 ;
         unsigned yincr = ( 4 < argc ) ? strtoul(argv[4],0,0) : 1 ;
         unsigned rgb   = ( 5 < argc ) ? strtoul(argv[5],0,0) : 0xFFFFFF ;

         fbDevice_t &fb = getFB();
         
         fb.clear(0,0,0);

         int const vsyncSignal = nextRtSignal();
         printf( "signal number %d\n", vsyncSignal );
         int const fdSync = open( "/dev/sm501vsync", O_RDONLY );

         walkingDot_t **dots = new walkingDot_t *[count];
         unsigned xPos = 0 ;
         unsigned yPos = 0 ;
         for( unsigned i = 0 ; i < count ; i++ ){
            dots[i] = new walkingDot_t( vsyncSignal, fb, fdSync, tpm, xPos, yPos, xincr, yincr, rgb );
            xPos++ ;
            xincr++ ;
//            yincr++ ;
         }

         fcntl(fdSync, F_SETOWN, getpid());
         fcntl(fdSync, F_SETSIG, vsyncSignal );
         int flags = flags = fcntl( fdSync, F_GETFL, 0 );
         fcntl( fdSync, F_SETFL, flags | O_NONBLOCK | FASYNC );

         while( 0 < numDots ){
            pause();
         }
      }
   }
   else
      fprintf( stderr, "Usage: %s #sprites [#ticksPerMove=1] [xincr=1] [yincr=1] [rgb=0xffff]\n", argv[0] );

   return 0 ;
}

#endif
