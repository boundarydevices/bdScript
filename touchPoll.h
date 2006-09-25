#ifndef __TOUCHPOLL_H__
#define __TOUCHPOLL_H__ "$Id: touchPoll.h,v 1.7 2006-09-25 18:50:30 ericn Exp $"

/*
 * touchPoll.h
 *
 * This header file declares the touchPoll_t 
 * class, which is used for polling the touch-screen
 * device.
 *
 *
 * Change History : 
 *
 * $Log: touchPoll.h,v $
 * Revision 1.7  2006-09-25 18:50:30  ericn
 * -add serial (MicroTouch EX II) touch support
 *
 * Revision 1.6  2006/08/07 16:47:23  tkisky
 * -...
 *
 * Revision 1.5  2005/11/17 03:47:53  ericn
 * -change default device name param to something recognizable internally
 *
 * Revision 1.4  2004/11/26 15:28:52  ericn
 * -use median/mean instead of range filter
 *
 * Revision 1.3  2004/01/01 20:11:42  ericn
 * -added isOpen() routine, and switched pollHandlers to use close()
 *
 * Revision 1.2  2003/11/24 19:09:07  ericn
 * -added deglitch
 *
 * Revision 1.1  2003/10/05 19:15:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "pollHandler.h"
typedef unsigned long kernel_ulong_t;
#define BITS_PER_LONG 32
#include <linux/types.h>
#include <linux/input.h>

class touchPollTimer_t ;

class touchPoll_t : public pollHandler_t {
public:
   touchPoll_t( pollHandlerSet_t &set,
                char const       *devName = 0 );
   ~touchPoll_t( void );

   // override this to perform processing of a touch
   virtual void onTouch( int x, int y, unsigned pressure, timeval const &tv );
   // override this to perform processing of a release
   virtual void onRelease( timeval const &tv );

   virtual void onDataAvail( void );

   void timeout( void );

   void dump( void );

protected:
   touchPollTimer_t *timer_ ;

   enum {
      findStart,
      byte0,
      byte1,
      byte2,
      byte3
   } state_t ;
   
   bool           isSerial_ ;
   unsigned       state_ ;
   bool           press_ ;
   unsigned short iVal_ ;
   unsigned short jVal_ ;

   char           inTrace_[256];
   unsigned       nextTrace_ ;
   unsigned       lastRead_ ;
   char           lastChar_ ;
};

#endif

