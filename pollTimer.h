#ifndef __POLLTIMER_H__
#define __POLLTIMER_H__ "$Id: pollTimer.h,v 1.1 2003-12-27 18:38:06 ericn Exp $"

/*
 * pollTimer.h
 *
 * This header file declares the pollTimer_t class, and
 * the getTimerPoll() routine to get a handle to the 
 * pipe used to implement it.
 *
 * The purpose of this module is to allow alarms to be
 * set and handled in conjunction with the generally poll()
 * oriented Javascript main thread.
 *
 * Poll timers are implemented using Posix timers 
 * ( timer_create(), etc..) and a pipe for poll(), so their
 * accuracy is on the order of a clock tick (10ms).
 * 
 * The getTimerPoll() routine must be called prior to instantiation
 * of any pollTimer_t objects.
 *
 * Change History : 
 *
 * $Log: pollTimer.h,v $
 * Revision 1.1  2003-12-27 18:38:06  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#ifndef __POLLHANDLER_H__
#include "pollHandler.h"
#endif 

class pollTimer_t {
public:
   pollTimer_t( void );
   virtual ~pollTimer_t( void );

   //
   // Use this and you'll get called back (through fire())
   // after the specified number of milliseconds
   //
   void set( unsigned long msFromNow );

   //
   // Unless you call this
   //
   void clear( void );

   //
   // Override this to do the right thing in response.
   // If you want another call, fire() should call set() again.
   // 
   virtual void fire( void );

protected:
   long long    expiration_ ;
   bool         isActive_ ;
   pollTimer_t *next_ ;

   friend class timerPollHandler_t ;
};

//
// used to initialize the poll
//
pollHandler_t &getTimerPoll( pollHandlerSet_t &set );

#endif

