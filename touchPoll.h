#ifndef __TOUCHPOLL_H__
#define __TOUCHPOLL_H__ "$Id: touchPoll.h,v 1.3 2004-01-01 20:11:42 ericn Exp $"

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
 * Revision 1.3  2004-01-01 20:11:42  ericn
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
#include <linux/input.h>

class touchPoll_t : public pollHandler_t {
public:
   touchPoll_t( pollHandlerSet_t &set,
                char const       *devName = "/dev/touchscreen/ucb1x00" );
   ~touchPoll_t( void );

   // override this to perform processing of a touch
   virtual void onTouch( int x, int y, unsigned pressure, timeval const &tv );
   // override this to perform processing of a release
   virtual void onRelease( timeval const &tv );

   virtual void onDataAvail( void );

   void setRange( unsigned x, unsigned y );
protected:
   int      prevX_ ;
   unsigned xRange_ ;
   long     xMotion_ ;
   int      prevY_ ;
   unsigned yRange_ ;
   long     yMotion_ ;
};

#endif

