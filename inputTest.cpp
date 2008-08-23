#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "inputPoll.h"
#include "touchCalibrate.h"
#include "fbDev.h"

class inputTouchScreen_t : public inputPoll_t {
public:
   // override this to handle keystrokes, button presses and the like
   inputTouchScreen_t( pollHandlerSet_t &set,
                       char const       *devName );
   virtual ~inputTouchScreen_t( void ){}

   inline bool haveCalibration( void ) const { return !raw_ ; }

   // called with calibrated point
   virtual bool onTouch( unsigned x,
                         unsigned y );
   virtual bool onRelease( void );

   // implementation
   virtual void onData( struct input_event const &event );

protected:
   bool      raw_ ;
   int       i_ ;
   int       j_ ;
   unsigned  mask_ ; // which have we received (I or J)
   bool      isDown_ ;
   bool      wasDown_ ;
};


inputTouchScreen_t::inputTouchScreen_t
   ( pollHandlerSet_t &set,
     char const       *devName )
   : inputPoll_t(set,devName)
   , raw_( true )
   , i_( 0 )
   , j_( 0 )
   , mask_( false )
   , isDown_( false )
   , wasDown_( false )
{
   raw_ = !touchCalibration_t::get().haveCalibration();
}

bool inputTouchScreen_t::onTouch( unsigned x, unsigned y )
{
   printf( "%u:%u\n", x, y );
}
bool inputTouchScreen_t::onRelease( void )
{
   printf( "---> release\n" );
}

static void cross( fbDevice_t &fb, point_t const &pt, unsigned char r, unsigned char g, unsigned char b ){
   unsigned left = pt.x >= 8 ? pt.x-8 : 0 ;
   unsigned right = pt.x < fb.getWidth()-8 ? pt.x+8 : fb.getWidth()-1 ;
   fb.line( left, pt.y, right, pt.y, 1, r, g, b );
   unsigned top = pt.y >= 8 ? pt.y-8 : 0 ;
   unsigned bottom = pt.y < fb.getHeight()-8 ? pt.y+8 : fb.getHeight()-1 ;
   fb.line( pt.x, top, pt.x, bottom, 1, r, g, b );
}

void inputTouchScreen_t::onData( struct input_event const &event )
{
   if( EV_ABS == event.type ){
      if( ABS_X == event.code ){
         i_ = event.value ;
         mask_ |= 1<<ABS_X ;
      } else if( ABS_Y == event.code ){
         j_ = event.value ;
         mask_ |= 1<<ABS_Y ;
      } else if( ABS_PRESSURE == event.code ){
         isDown_ = (0 != event.value);
	 if( !isDown_ ){
		 mask_ = 0 ;
	 }
      }
   } else if( EV_SYN == event.type ){
      if( (isDown_ != wasDown_) && (!isDown_) ){
         wasDown_ = isDown_ ;
         onRelease();
         mask_ = 0 ;
      } else if( mask_ == ((1<<ABS_X)|(1<<ABS_Y)) ){
         if( isDown_ ){
            if( !raw_ ){
               point_t raw ; raw.x = i_ ; raw.y = j_ ;
               point_t cooked ;
               touchCalibration_t::get().translate(raw,cooked);
               fbDevice_t &fb = getFB();
               i_ = cooked.x ; if( 0 > i_ ) i_ = 0 ; if( i_ >= fb.getWidth() ) i_= fb.getWidth()-1 ;
               j_ = cooked.y ; if( 0 > j_ ) j_ = 0 ; if( j_ >= fb.getHeight() ) j_ = fb.getHeight() - 1 ;
               cooked.x = i_ ;
               cooked.y = j_ ;
               cross( getFB(), cooked, 0xff, 0xff, 0xff );
               printf( "raw: %u:%u -> cooked %u:%u\n", raw.x, raw.y, cooked.x, cooked.y );
            }
            onTouch(i_,j_);
         }
         mask_ = 0 ; i_ = j_ = 0 ;
         wasDown_ = true ;
      }
   } else {
      printf( "type %d, code 0x%x, value %d\n",
            event.type, 
            event.code,
            event.value );
   }
}

#define DEVENV "TSDEV" 

int main( int argc, char const * const argv[] )
{
   char const *device = getenv(DEVENV);
   if( !device ){
      device="/dev/input/event0" ;
   }
   printf( "opening %s\n", device );
   
   pollHandlerSet_t handlers ;
   inputTouchScreen_t touchScreen( handlers, device );
   if( touchScreen.isOpen() ){
      while( 1 ){
             handlers.poll( -1 );
      }
   }
   
   return 0 ;
}
