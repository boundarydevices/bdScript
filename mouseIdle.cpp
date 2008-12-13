/*
 * mouseIdle
 *
 * This program executes a command when the mouse is idle for a 
 * specified period of time, and another when it becomes "busy"
 *
 * Usage is:
 *
 *    mouseIdle timeout_in_ms    idleCommand       activeCommand
 *
 */

#include <stdio.h>
#include "inputDevs.h"
#include <unistd.h>
#include <stdlib.h>
#include "inputMouse.h"
#include "tickMs.h"

class myMouse_t : public inputMouse_t {
public:
   myMouse_t( pollHandlerSet_t &set, char const *devName );
   virtual ~myMouse_t( void );
   virtual void onData( struct input_event const &event );

   long long lastActive_ ;
};

myMouse_t::myMouse_t( pollHandlerSet_t &set, char const *devName )
   : inputMouse_t( set, devName )
   , lastActive_( tickMs() )
{
}

myMouse_t::~myMouse_t( void )
{
}

void myMouse_t::onData( struct input_event const &event )
{
   lastActive_ = tickMs();
}


int main( int argc, char const * const argv[] )
{
   if( 4 == argc ){
      unsigned long timeout = strtoul(argv[1],0,0);
      if( 0 < timeout ){
         inputDevs_t idevs ;
         int mouseDev = -1 ;
         for( unsigned i = 0 ; i < idevs.count(); i++ ){
            inputDevs_t::type_e type ;
            unsigned idx ;

            idevs.getInfo(i, type, idx);

            if( inputDevs_t::MOUSE == type ){
               mouseDev = idx ;
               break;
            }
         }
         
         if( 0 <= mouseDev ){
            char devName[512];
            snprintf(devName,sizeof(devName), "/dev/input/event%u", mouseDev );

            printf( "mouse found as %s\n", devName );
            pollHandlerSet_t handlers ;
            myMouse_t mouse(handlers,devName);

            char const * const activeCommand = argv[3];
            char const * const idleCommand = argv[2];
            bool active = true ;
            long long lastActive = tickMs();

            system(activeCommand);
            while( 1 ){
               handlers.poll(1000);
               unsigned idleTime = (unsigned)(tickMs()-mouse.lastActive_);
               if( active && (idleTime > timeout) ){
                  lastActive = mouse.lastActive_ ;
                  printf( "%s\n", idleCommand );
                  system(idleCommand);
                  active = false ;
               }
               else if( !active && (lastActive != mouse.lastActive_) ){
                  printf( "%s\n", activeCommand );
                  system(activeCommand);
                  active = true ;
               }
               lastActive = mouse.lastActive_ ;
            }
         }
         else
            fprintf( stderr, "No mouse\n" );
      }
      else
         fprintf( stderr, "Invalid timeout %lu\n", timeout );
   }
   else
      fprintf( stderr, "Usage: %s\ttimeout_ms\tidleCommand\tactiveCommand\n", argv[0] );
   return 0 ;
}
