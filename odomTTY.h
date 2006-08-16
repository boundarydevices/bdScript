#ifndef __ODOMTTY_H__
#define __ODOMTTY_H__ "$Id: odomTTY.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * odomTTY.h
 *
 * This header file declares the odomTTY_t class,
 * which is used to provide a non-blocking interface
 * to stdin.
 *
 * All lines are pumped to the provided command interpreter.
 *
 * Change History : 
 *
 * $Log: odomTTY.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "ttyPoll.h"
#include "odomCommand.h"

class odomTTY_t : public ttyPollHandler_t {
public:
   odomTTY_t( pollHandlerSet_t  &set,
              odomCmdInterp_t   &interp );
   ~odomTTY_t( void );

   virtual void onLineIn( void );
   virtual void onCtrlC( void );

protected:
   odomCmdInterp_t &interp_ ;
};

#endif

