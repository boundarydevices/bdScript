#ifndef __INPUTPOLL_H__
#define __INPUTPOLL_H__ "$Id: inputPoll.h,v 1.1 2007-07-07 00:25:52 ericn Exp $"

/*
 * inputPoll.h
 *
 * This header file declares the inputPoller_t class, 
 * which polls for input events.
 *
 *
 * Change History : 
 *
 * $Log: inputPoll.h,v $
 * Revision 1.1  2007-07-07 00:25:52  ericn
 * -[bdScript] added mouse cursor and input handling
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#include "pollHandler.h"
#include <linux/input.h>

class inputPoll_t : public pollHandler_t {
public:
   inputPoll_t( pollHandlerSet_t &set,
                char const       *devName );
   ~inputPoll_t( void );

   // override this to handle keystrokes, button presses and the like
   virtual void onData( struct input_event const &event );

   // implementation
   virtual void inputPoll_t::onDataAvail( void );
protected:
   char const *fileName_ ;
};

#endif

