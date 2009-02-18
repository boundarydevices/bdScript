/*
 * Module touchPoll.cpp
 *
 * This module defines the methods of the touchPoll_t
 * class as declared in touchPoll.h
 *
 *
 * Change History : 
 *
 * $Log: touchPoll.cpp,v $
 * Revision 1.16  2009-02-18 19:11:41  ericn
 * initial import
 *
 * Revision 1.15  2008-09-11 00:26:51  ericn
 * -[touchPoll] Implement new test program
 *
 * Revision 1.14  2006-12-01 18:25:32  tkisky
 * -change default touchscreen device name
 *
 * Revision 1.13  2006/09/25 19:19:50  ericn
 * -remove debug msg
 *
 * Revision 1.12  2006/09/25 18:50:34  ericn
 * -add serial (MicroTouch EX II) touch support
 *
 * Revision 1.11  2006/05/14 14:32:42  ericn
 * -use first timestamp, not last
 *
 * Revision 1.10  2005/11/23 13:08:20  ericn
 * -printf, not debugPrint in base class onTouch/onRelease
 *
 * Revision 1.9  2005/11/17 03:48:26  ericn
 * -allow environment override of device, change default to /dev/touch_ucb1x00
 *
 * Revision 1.8  2004/12/28 03:48:01  ericn
 * -clean queue on exit
 *
 * Revision 1.7  2004/12/03 04:43:50  ericn
 * -use median/mean filters
 *
 * Revision 1.6  2004/11/26 15:28:54  ericn
 * -use median/mean instead of range filter
 *
 * Revision 1.5  2004/01/01 20:11:42  ericn
 * -added isOpen() routine, and switched pollHandlers to use close()
 *
 * Revision 1.4  2003/11/28 14:04:48  ericn
 * -removed debug msgs
 *
 * Revision 1.3  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.2  2003/11/24 19:09:13  ericn
 * -added deglitch
 *
 * Revision 1.1  2003/10/05 19:15:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "touchPoll.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "setSerial.h"
#include "pollTimer.h"
#include "config.h"

// #define DEBUGPRINT
#include "debugPrint.h"
#include "rollingMedian.h"
#include "rollingMean.h"
#include "tickMs.h"

#define MEDIANRANGE 5
static rollingMedian_t medianX_( MEDIANRANGE );
static rollingMedian_t medianY_( MEDIANRANGE );

#define MEANRANGE 5
static rollingMean_t meanX_( MEANRANGE );
static rollingMean_t meanY_( MEANRANGE );

#ifdef KERNEL_INPUT_TOUCHSCREENx
#include <linux/input.h>
#define MAX_EVENTS_PER_READ 16
#define HAVE_X (1<<ABS_X)
#define HAVE_Y (1<<ABS_Y)
#define ISDOWN (1<<ABS_Z)
#define WASDOWN (1<<ABS_RUDDER)
#else
struct ts_event  {   /* Used in UCB1x00 style touchscreens (the default) */
	unsigned short pressure;
	unsigned short x;
	unsigned short y;
	unsigned short pad;
	struct timeval stamp;
};
#endif

static char const *getTouchDev( char const *devName )
{
   char const *envDev = getenv( "TSDEV" );
   if( 0 != envDev )
      devName = envDev ;
   if( 0 == devName )
#ifdef KERNEL_INPUT_TOUCHSCREENx
      devName = "/dev/input/event0" ;
#else
      devName = "/dev/touch_ucb1x00" ;
#endif
   return devName ;
}

static bool isSerial( char const *devName ){
   return 0 != strstr( devName, "ttyS" );
}

static int openTouchDev( char const *devName ){
   int fd = -1 ;
   if( !isSerial(devName) ){
      fd = open(devName, O_RDONLY);
   }
   else {
      printf( "Serial touch screen\n" );
      char const *end = strchr( devName, ',' );
      if( 0 == end )
         end = devName + strlen(devName);
      unsigned nameLen = end-devName ;
      printf( "nameLen: %u, end %p\n", nameLen, end );
      char deviceName[512];
      if( nameLen < sizeof(deviceName) ){
         memcpy( deviceName, devName, nameLen );
         deviceName[nameLen] = '\0' ;
         unsigned baud = 9600 ;
         unsigned databits = 8 ;
         char parity = 'N' ;
         unsigned stop = 1 ;
         if( '\0' != *end ){
            end++ ;
            baud = 0 ; 
            while( isdigit(*end) ){
               baud *= 10 ;
               baud += ( *end-'0' );
               end++ ;
            }

            if( ',' == *end ){
               end++ ;
               databits = *end-'0' ;
               end++ ;
               if( ',' == *end ){
                  end++ ;
                  parity = *end++ ;
                  if( ',' == *end ){
                     stop = end[1] - '0';
                  }
               }
            }
         }
         fd = open( deviceName, O_RDWR );
         if( 0 < fd ){
            printf( "settings: %s,%u,%u,%c,%u\n", deviceName, baud, databits, parity, stop );
            setBaud( fd, baud );
            setRaw( fd );
            setDataBits( fd, databits );
            setStopBits( fd, stop );
            setParity( fd, parity );
         }
         else
            perror( deviceName );
      }
      else
         fprintf( stderr, "Invalid touch device name\n" );
   }

   return fd ;
}

class touchPollTimer_t : public pollTimer_t {
public:
   touchPollTimer_t( touchPoll_t &bcp )
      : touch_( bcp ){}

   virtual void fire( void ){ touch_.timeout(); }

private:
   touchPoll_t &touch_ ;
};

touchPoll_t :: touchPoll_t
   ( pollHandlerSet_t &set,
     char const       *devName )
   : pollHandler_t( openTouchDev( getTouchDev(devName) ), set )
   , timer_( 0 )
   , isSerial_( isSerial(getTouchDev(devName)) )
   , state_( findStart )
   , iVal_( 0 )
   , jVal_( 0 )
   , nextTrace_( 0 )
   , lastRead_( 0 )
   , lastChar_( 0 )
{
debugPrint( "touchPoll constructed, dev = %s, fd == %d\n", getTouchDev(devName), getFd() );   
   if( isOpen() )
   {
      fcntl( fd_, F_SETFD, FD_CLOEXEC );
      fcntl( fd_, F_SETFL, O_NONBLOCK );
      setMask( POLLIN );
      set.add( *this );
      if( isSerial_ )
         timer_ = new touchPollTimer_t( *this );
   }
}

touchPoll_t :: ~touchPoll_t( void )
{
   if( isOpen() )
   {
#ifdef KERNEL_INPUT_TOUCHSCREENx
#else
      ts_event event ;
      int numRead ;
   
      while( sizeof( event ) == ( numRead = read( fd_, &event, sizeof( event ) ) ) )
         ; // purge queue
      close();
#endif
   }

   if( timer_ ){
      delete timer_ ;
   }
}

void touchPoll_t :: onTouch( int x, int y, unsigned pressure, timeval const &tv )
{
   printf( "touch screen touch : %u/%u, pressure %u\n", x, y, pressure );
}

void touchPoll_t :: onRelease( timeval const &tv )
{
   printf( "touch screen release\n" );
}

static char const * const stateNames_[] = {
   "findStart",
   "byte0",
   "byte1",
   "byte2",
   "byte3"
};

static bool wasDown = false ;

void touchPoll_t :: onDataAvail( void )
{
   debugPrint( "onDataAvail() %s\n", 
               isSerial_ 
                  ? "serial" 
#ifdef KERNEL_INPUT_TOUCHSCREENx
                  : "input" );
#else
                  : "ucb1x00" );
#endif

   if( !isSerial_ ){
#ifdef KERNEL_INPUT_TOUCHSCREENx
	struct input_event events[MAX_EVENTS_PER_READ];
	do {
		int bytesRead = read( fd_, events, sizeof(events) );
		if( 0 < bytesRead ){
		    int itemsRead = bytesRead / sizeof(events[0]);
		    unsigned leftover = bytesRead % sizeof(events[0]);
		    if( leftover ){
			    unsigned left = sizeof(events[0])-leftover ;
			    bytesRead = read( fd_, ((char *)events)+bytesRead, left );
			    if( bytesRead == (int)left ){
				    itemsRead++ ;
			    }
		    } // try to read tail-end
                    
                    debugPrint( "%u input events read\n", itemsRead );
		    for( int i = 0 ; i < itemsRead ; i++ ){
                        struct input_event &event = events[i];
                        if( EV_ABS == event.type ){
                           if( ABS_X == event.code ){
                              iVal_ = event.value ;
                              mask_ |= HAVE_X ;
                           } else if( ABS_Y == event.code ){
                              jVal_ = event.value ;
                              mask_ |= HAVE_Y ;
                           } else if( ABS_PRESSURE == event.code ){
                              if(0 != event.value)
                                 mask_ |= ISDOWN ;
                              else
                                 mask_ &= ~ISDOWN ;
                           }
                        } else if( EV_SYN == event.type ){
                           bool isDown = 0 != (mask_ & ISDOWN);
                           bool wasDown = 0 != (mask_ & WASDOWN);
                           if( (isDown != wasDown) && (!isDown) ){
                              if( isDown )
                                 mask_ |= WASDOWN ;
                              else
                                 mask_ &= ~WASDOWN ;
         
                              struct timeval stamp ;
                              gettimeofday( &stamp, 0 );
                              onRelease( stamp );
                              mask_ &= ~(HAVE_X|HAVE_Y); // no meaning on release
                           } else if( (HAVE_X|HAVE_Y) == (mask_ & (HAVE_X|HAVE_Y) ) ){
                              medianX_.feed( iVal_ );
                              medianY_.feed( jVal_ );
   
                              unsigned short x, y ;
                              if( medianX_.read(x) && medianY_.read(y) ){
                                 struct timeval stamp ;
                                 gettimeofday( &stamp, 0 );
                                 onTouch(x,y,1,stamp);
                                 mask_ |= WASDOWN ;
                              }
                              mask_ &= ~(HAVE_X|HAVE_Y); iVal_ = jVal_ = 0 ;
                           }
   		        }
                    } // for each input event
		}
		else
			break ;
	} while( 1 );
#else
      ts_event nextEvent ;
      ts_event event ;
      unsigned count = 0 ;
      int numRead ;
   
      timeval firstTime ;

      while( sizeof( nextEvent ) == ( numRead = read( fd_, &nextEvent, sizeof( nextEvent ) ) ) )
      {
         if( 0 == count )
            firstTime = nextEvent.stamp ;
   
         medianX_.feed( nextEvent.x );
         medianY_.feed( nextEvent.y );
   
debugPrint( "sample: %u/%u\n", nextEvent.x, nextEvent.y );
#ifdef XDEBUGPRINT
      printf( "medianX: " ); medianX_.dump();
      printf( "medianY: " ); medianY_.dump();
#endif
   
         unsigned short sampleX ;
         if( medianX_.read( sampleX ) )
            meanX_.feed( sampleX );
         unsigned short sampleY ;
         if( medianY_.read( sampleY ) )
            meanY_.feed( sampleY );
         event = nextEvent ;
         count++ ;
      }
      
      if( 0 < count )
      {
         if( 0 < event.pressure )
         {
            unsigned short x, y ;
   //         x = event.x ; y = event.y ; if( 1 )
   //         if( medianX_.read( x ) && medianY_.read( y ) )
            if( meanX_.read( x ) && meanY_.read( y ) )
            {
   debugPrint( "out: %u/%u\n", x, y );
               onTouch( x, y, event.pressure, firstTime );
               if( !wasDown )
               {
   debugPrint( "first touch at %u:%u\n", x, y );
                  wasDown = true ;
               }
            }
            else
            {
   debugPrint( "not enough values\n" );
            }
         }
         else
         {
            if( wasDown )
            {
               onRelease( firstTime );
               wasDown = false ;
            }
            else
            {
   debugPrint( "missed touch\n" );
            }
            medianX_.reset(); medianY_.reset();
            meanX_.reset(); meanY_.reset();
         }
      }
#endif
   }
   else {
      if( timer_ )
         timer_->clear();

#ifdef KERNEL_INPUT_TOUCHSCREENx
#else
      ts_event event ;
      char     inBuf[512];
      int      numRead ;
      unsigned count = 0 ;

      unsigned short *values[] = {
         &iVal_,
         &jVal_
      };

      while( 0 < ( numRead = read( fd_, &inBuf, sizeof( inBuf ) ) ) ){
         debugPrint( "read %u bytes\n", numRead );
         lastRead_ = nextTrace_ ;
         for( int i = 0 ; i < numRead ; i++ ){
            char const c = inBuf[i];

            debugPrint( "char[%u] %02x, state %s\n", i, c, stateNames_[state_] );

            lastChar_ = c ;
            unsigned t = nextTrace_ & (sizeof(inTrace_)-1);
            nextTrace_++ ;
            inTrace_[t] = c ;

            switch( state_ ){
               case findStart: {
                  if( c & 0x80 ){
                     state_++ ;
                     iVal_ = jVal_ = 0 ;
                     press_ = ( 0 != ( c & 0x40 ) );
                     debugPrint( "%s\r\n", press_ ? "press" : "release" );
                  }
                  else
                     debugPrint( "Non-leading <%02x>\n", c );
                  break ;
               }

               default: {
                  if( 0 != ( c & 0x80 ) ){
                     state_ = findStart ;
                     --i ;
                  }
                  else {
                     unsigned offs = state_-byte0 ;
                     unsigned short &val = *values[offs>>1];
                     if( 0 == ( offs & 1 ) )
                        val = c & 0x7f ;
                     else
                        val += (c<<7);
                     state_++ ;
                     if( byte3 < state_ ){
                        state_ = findStart ;
                        event.x = iVal_ ;
                        event.y = jVal_ ;
                        event.pressure = press_ ;
                        medianX_.feed( event.x );
                        medianY_.feed( event.y );
                     }
                     count++ ;
                  }
               }
            }
         }
      } // while more input data
      
      if( count ){
         debugPrint( "%s %04x:%04x\r\n", event.pressure ? "press" : "release", iVal_, jVal_ );
         gettimeofday( &event.stamp, 0 );
         if( event.pressure ){
            if( medianX_.read( event.x ) && medianY_.read( event.y ) ){
               onTouch( event.x, event.y, 1, event.stamp );
               wasDown = true ;
            }
            else
               debugPrint( "Not enough samples\n" );
         }
         else {
            onRelease( event.stamp );
            medianX_.reset(); medianY_.reset();
            wasDown = false ;
         }
      }
      if( wasDown && ( 0 != timer_ ) )
         timer_->set( 100 );
#endif
   }
}

void touchPoll_t :: timeout( void )
{
   printf( "touch timeout\n" );
   if( wasDown ){
      struct timeval stamp ;
      gettimeofday( &stamp, 0 );
      onRelease( stamp );
      medianX_.reset(); medianY_.reset();
      wasDown = false ;
   }
}

void touchPoll_t :: dump( void )
{
   unsigned t = 0 ;
   unsigned count = sizeof( inTrace_ );
   if( nextTrace_ >= sizeof(inTrace_) ){
      t = nextTrace_ & (sizeof(inTrace_)-1);
   }
   else
      count = nextTrace_ ;

   printf( "--> %u chars traced, %u read\n", count, nextTrace_ );
   printf( "    last read %u, char <%02x>\n", lastRead_, lastChar_ );
   printf( "    state %s, press %u\n", stateNames_[state_], press_ );
   printf( "    iVal %04x\n", iVal_ );
   printf( "    jVal %04x\n", jVal_ );
   unsigned pos = 0 ;
   while( count-- ){
      printf( "%02x ", inTrace_[t] );
      if( 16 == ++pos ){
         pos = 0 ;
         printf( "\n" );
      }
      t = ( t + 1 ) & (sizeof(inTrace_)-1);
   }
   printf( "\n" );
}

#ifdef STANDALONE

#include <unistd.h>
#include "pollTimer.h"

int main( void )
{
   pollHandlerSet_t handlers ;

   getTimerPoll(handlers);

   touchPoll_t      touchPoll( handlers );
   if( touchPoll.isOpen() )
   {
      printf( "opened touchPoll: fd %d, mask %x\n", touchPoll.getFd(), touchPoll.getMask() );

      int iterations = 0 ;
      while( 1 )
      {
         handlers.poll( -1 );
//         debugPrint( "poll %d\n", ++iterations );
      }
   }
   else
      perror( "error opening touch device" );

   return 0 ;
}

#endif
