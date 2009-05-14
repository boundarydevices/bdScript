#include <stdio.h>
#include "fbcCircular.h"
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
#include "rawKbd.h"
#include <ctype.h>
#include <math.h>
#include "cylinderShadow.h"

enum digitState_e {
   WAITTICK,         // wait for pull
   BACK,             // backup
   LAUNCH,           // accelerate
   SPIN,             // free-wheel - this is variable to set target
   DECEL,            // decelerate to initial bounce velocity
   BOUNCE,           // bounce into position
   DONE         
};

static char const * const stateNames[] = {
   "WAITTICK",
   "BACK",
   "LAUNCH",
   "SPIN",
   "DECEL",
   "BOUNCE",
   "DONE" 
};

typedef struct {
   unsigned       targetOffs_ ;
   digitState_e   state_ ;
   unsigned       startTick_ ;
   unsigned       startPos_ ;
   unsigned       targetDigit_ ;
   unsigned       startDecel_ ;
   int            v_ ;
} digitContext_t ;

static fbPtr_t           *cmdListMem_ = 0 ;
static fbCmdListSignal_t *cmdListDev_ = 0 ;
static sig_atomic_t vsyncCount = 0 ;
static sig_atomic_t completeCount = 0 ;

static void videoOutput( int signo, void *param )
{
   ++vsyncCount;
}

static void cmdListComplete( int signo, void *param )
{
//   printf( "cmp\n" );
   ++completeCount ;
}

static char const digitImgFile[] = {
   "/tradeShow/elements.png"
};

#define NUMDIGITS 4
#define HEIGHT    400
#define STARTX    0
#define STARTY    (768-HEIGHT)
#define HZ        60

#define BACKVELOC -8
#define BACKTICKS 30

#define ACCELA     6
#define ACCELV     80
#define ACCELTICKS 180

#define SPINTICKS ((3*HZ)/2)

#define BOUNCEV     60
#define BOUNCETICKS ((3*HZ)/2)

#define NUMBOUNCES  3

// total DECEL + BOUNCE movement
//    needs tweaking when items above are changed
// 
#define DECELOFFS    1390

/*
 * Bounce equations start by choosing a multiplier that matches the initial
 * velocity (slope). Since d/dx(sin(0)) == 1, multiplying by the initial
 * velocity gives a sine wave of the right magnitude.
 *
 *    BOUNCEV*sin(x)
 *
 * Note that x is a function of time, and higher values will give faster
 * bounces, so we'll need a multiplier (or divider of t). Let's say we want
 * four complete sine waves during the interval [0..BOUNCETICKS].
 *
 * We start with the general equation (t0 is the transition time from DECEL
 * to BOUNCE):
 *
 *    x = (t-t0)*mult
 *
 * and we want 
 *    x == 4*(2*PI)
 * when
 *    t-t0  == BOUNCETICKS
 *
 * so we get
 *
 *    4*(2*PI) == BOUNCETICKS*mult
 *
 * so
 *
 *    mult == (8*PI)/BOUNCETICKS
 *
 * and the equation for the wave (bounce) becomes:
 *
 *    wave(t)   = BOUNCEV*sin((t-t0)*((NUMBOUNCES*2*PI)/BOUNCETICKS))
 *
 * But we're not done. We want the bounces to dampen over the period [t0..t0+BOUNCETICKS].
 * For now, we'll do this linearly, scaling the previous pos() function
 * by the distance of t to the end point.
 *
 * Let's define a function 'frac' that scales from 1 to zero along
 * this range:
 *
 *    frac(t)  = ((BOUNCETICKS-(t-t0)))/BOUNCETICKS)
 *
 * Putting the parts together, we get:
 *
 *    pos(t) = wave(t)*frac(t)
 */
static double wave(unsigned t) // t-t0
{
   return BOUNCEV*sin((t*NUMBOUNCES*2*M_PI)/BOUNCETICKS);
}

static double frac(unsigned t) // t-t0
{
   return ((double)(BOUNCETICKS-t))/BOUNCETICKS ;
}

static int bouncePos(unsigned t) // t-t0
{
   return (int)round(wave(t)*frac(t));
}

static bool fixup( digitContext_t  *context,
                   fbcCircular_t  **wheels,
                   unsigned         offs )
{
   bool alldone = true ;
   for( unsigned i = 0 ; i < NUMDIGITS ; i++ ){
      digitContext_t &ctx = context[i];
      fbcCircular_t  &wheel = *wheels[i];
      wheel.executed();

      alldone = alldone && (DONE == ctx.state_);
      switch(ctx.state_){
         case WAITTICK: {
            if( offs >= (i*HZ) ){
               ctx.startTick_ = offs ;
               ctx.state_ = BACK ;
               ctx.startPos_ = wheel.getOffset();
               wheel.setOffset(0);
            }
            break;
         }
         case BACK: {
            if( 0 == ctx.v_ )
               ctx.v_ = -1 ;
            else if( BACKVELOC < ctx.v_ ){
               ctx.v_ *= 2 ;
               if( ctx.v_ < BACKVELOC ){
                  ctx.v_ = BACKVELOC ;
               }
            }
            int reloffs = wheel.getOffset()+ctx.v_ ;
            while( 0 > reloffs ){
               reloffs += wheel.getHeight();
            }
            wheel.setOffset(reloffs);
            reloffs = offs - ctx.startTick_ ;
            if( reloffs > BACKTICKS ){
               ctx.startTick_ = offs ;
               ctx.startPos_ = wheel.getOffset();
               ctx.state_ = LAUNCH ;
printf( "%s at tick %u\n", stateNames[ctx.state_], ctx.startTick_ );
            }
            break;
         }
         case LAUNCH: {
            if( 0 > ctx.v_ ){
               ctx.v_ /= 2 ; // slow down backward movement
//               printf( "slow down v == %d\n", ctx.v_ );
            } else if( ACCELV > ctx.v_ ){
               ctx.v_ += ACCELA ;
               if( ctx.v_ > ACCELV ){
                  ctx.v_ = ACCELV ;
               }
//               printf( "accelerate v == %d\n", ctx.v_ );
            }
            int reloffs = wheel.getOffset()+ctx.v_ ;
            while( 0 > reloffs ){
               reloffs += wheel.getHeight();
            }
            wheel.setOffset(reloffs);
            reloffs = offs - ctx.startTick_ ;
            if( (unsigned)reloffs > ACCELTICKS ){
               ctx.startPos_ = wheel.getOffset();
               ctx.startTick_ = offs ;
               ctx.state_ = SPIN ;
printf( "%s at tick %u\n", stateNames[ctx.state_], ctx.startTick_ );
            }
            break;
         }
         case SPIN: {
            unsigned reloffs = offs - ctx.startTick_ ;
            if( reloffs >= SPINTICKS ){
               int pos = (ctx.targetOffs_-DECELOFFS);
               while( 0 > pos )
                  pos += wheel.getHeight();
printf( "stop SPIN at %u\n", pos );
               wheel.setOffset(pos);
               ctx.startPos_ = ctx.startDecel_ = pos ;
               ctx.startTick_ = offs ;
               ctx.state_ = DECEL ;
printf( "%s at tick %u\n", stateNames[ctx.state_], ctx.startTick_ );
            }
            else {
               wheel.setOffset( wheel.getOffset()+ctx.v_ );
            }
            break;
         }
         case DECEL: {
            if( BOUNCEV < ctx.v_ ){
               --ctx.v_ ;
            }
            wheel.setOffset(wheel.getOffset() + ctx.v_ );
            if(BOUNCEV == ctx.v_){
               int relPix = wheel.getOffset()-ctx.startPos_ ;
               while( 0 > relPix ){
                  relPix += wheel.getHeight();
               }
               ctx.startPos_  = wheel.getOffset();
               ctx.startTick_ = offs ;
               ctx.state_ = BOUNCE ;
printf( "%s at tick %u\n", stateNames[ctx.state_], ctx.startTick_ );
printf( "DECEL pixels = %d\n", relPix );
            }
            break;
         }
         case BOUNCE: {
            unsigned reloffs = offs-ctx.startTick_ ;
            if( reloffs > BOUNCETICKS )
               reloffs = BOUNCETICKS ;
            int bounce = bouncePos(reloffs)+ctx.startPos_ ;
            while( 0 > bounce )
               bounce += wheel.getHeight();
            wheel.setOffset(bounce);
            if( reloffs >= BOUNCETICKS ){
               int relPix = wheel.getOffset()-ctx.startDecel_ ;
               while( 0 > relPix ){
                  relPix += wheel.getHeight();
               }
               ctx.startPos_ = bounce ;
               ctx.startTick_ = offs ;
               ctx.state_ = DONE ;
printf( "%s at tick %u\n", stateNames[ctx.state_], ctx.startTick_ );
printf( "DECEL+BOUNCE offset %u\n", relPix );
            }
            break;
         }
         case DONE: {
         }
      }
      wheel.updateCommandList();
   }
   return alldone ;
}

int main( int argc, char const * const argv[] )
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

   sigaddset( &blockThese, cmdListDev.getSignal() );
   sigaddset( &blockThese, vsync.getSignal() );

   setSignalHandler( vsync.getSignal(), blockThese, videoOutput, 0 );
   setSignalHandler( cmdListDev.getSignal(), blockThese, cmdListComplete, 0 );

   image_t digitImg ;
   if( !imageFromFile( digitImgFile, digitImg ) ){
      perror( digitImgFile );
      return -1 ;
   }
   printf( "%s: %ux%u at %u:%u\n", digitImgFile, digitImg.width_, digitImg.height_, STARTX, STARTY );

   unsigned const lmargin = (fb.getWidth()-(NUMDIGITS*digitImg.width_))/2 ;
   unsigned digitHeight = digitImg.height_ / 10 ;
   unsigned const dig_margin = (HEIGHT-digitHeight) / 2 ;
   unsigned const initPos = (digitImg.height_ - dig_margin);
   printf( "digit height is %u, dig_margin is %u\n", digitHeight, dig_margin );
   fbImage_t fbDigitImg(digitImg, fbImage_t::rgb565 );

   fbPtr_t shadow = cylinderShadow( digitImg.width_, HEIGHT, 2.0, 1.0, HEIGHT*15/16, HEIGHT/32 );
   fbImage_t shadowImg( shadow, digitImg.width_, HEIGHT );
   sm501alpha_t &alphaLayer = sm501alpha_t::get( sm501alpha_t::rgba4444 ); // , STARTX+lmargin, STARTY, NUMDIGITS*digitImg.width_, HEIGHT );

   fbcCircular_t *wheels[NUMDIGITS];
   unsigned x = STARTX+lmargin;
   for( unsigned i = 0 ; i < NUMDIGITS ; i++ ){
      wheels[i] = new fbcCircular_t( cmdList, fb.fb0_offset(), x, STARTY, 
                                      fb.getWidth(), fb.getHeight(), 
                                      fbDigitImg,
                                      HEIGHT, initPos, false, 0x000000 );
      printf( "set wheelpos[%u] to %u\n", i, digitImg.height_ - dig_margin);
      wheels[i]->show();
      x += digitImg.width_ ;
   }
   
   cmdList.push( new fbFinish_t );
   printf( "%u bytes of cmdlist\n", cmdList.size() );

   fbPtr_t cmdListMem( cmdList.size() );
   cmdList.copy( cmdListMem.getPtr() );

   cmdListMem_ = &cmdListMem ;
   cmdListDev_ = &cmdListDev ;

   vsync.enable();
   cmdListDev.enable();
   
   digitContext_t digitStates[NUMDIGITS];
   memset(digitStates,0,sizeof(digitStates));
   fixup(digitStates,wheels,0);
   unsigned offset = cmdListMem.getOffs();
   int numWritten = write( cmdListDev.getFd(), &offset, sizeof( offset ) );
   if( numWritten != sizeof( offset ) ){
      perror( "write(cmdListDev)" );
      exit(1);
   }
   do {
      printf( "digits: " );
      char inBuf[80];
      fflush(stdout); fflush(stderr);
      if( fgets(inBuf,sizeof(inBuf),stdin) ){
         unsigned len = strlen(inBuf);
         if( NUMDIGITS+1 == len ){
            memset(digitStates,0,sizeof(digitStates));

            bool valid = true ;
            x = STARTX+lmargin ;
            for( unsigned i = 0 ; i < NUMDIGITS ; i++ ){
               char c = inBuf[i];
               if( ('0' <= c) && ('9' >= c) ){
                  int pos = (c-'0')*digitHeight - dig_margin;
                  while( 0 > pos ){
                     pos += digitImg.height_ ;
                  }
                  alphaLayer.draw4444( (unsigned short *)shadow.getPtr(), x, STARTY, digitImg.width_, HEIGHT);
                  digitStates[i].targetOffs_ = pos ;
                  x += digitImg.width_ ;
               }
               else {
                  valid = false ;
                  break;
               }
            }
            if( !valid ){
               fprintf( stderr, "Invalid digit string %s\n", inBuf );
               continue;
            }
            
            sig_atomic_t writeCount = completeCount ;
            unsigned long startTick = vsyncCount ;
            bool alldone = false ;
            while( !alldone ){
               if( writeCount == completeCount ){
                  unsigned long vtick = vsyncCount - startTick ;
                  alldone = fixup(digitStates,wheels,vtick);
                  offset = cmdListMem.getOffs();
                  numWritten = write( cmdListDev.getFd(), &offset, sizeof( offset ) );
                  if( numWritten != sizeof( offset ) ){
                     perror( "write(cmdListDev)" );
                     exit(1);
                  }
                  writeCount++ ;
               } else {
                  pause();
               }
            }
         }
      }
   } while( 1 );
   
   return 0 ;
}
