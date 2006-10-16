#ifndef __ASYNCSCREENOBJ_H__
#define __ASYNCSCREENOBJ_H__ "$Id: asyncScreenObj.h,v 1.1 2006-10-16 22:45:26 ericn Exp $"

/*
 * asyncScreenObj.h
 *
 * This header file declares the asyncScreenObject_t class to
 * enforce a standard for updating on-screen objects 
 * displayed by the SM-501's command-list interpreter.
 *
 * The general notion is that an on-screen object contains
 * three pieces of state:
 *
 *    1. Its' on-screen state
 *    2. The state of its' command in a command-list
 *    3. Its' desired state
 *
 * Updates to 1 and 2 must be synchronized such that the
 * command-list interpreter is not running while the command
 * list entry is being fixed up.
 *
 * Updates to number 2 and three must also be synchronized
 * so that the desired state is not partially updated during
 * the command-list fixup.
 *
 * In general, the on-screen state is updated by the command
 * list interpreter from the vertical-sync signal by sending
 * the command-list to the SM-501 driver, and completed is
 * signalled through command-list-complete signal. State #2
 * is normally updated in the command-list-complete handler.
 *
 * State #3 is normally set by the application in either main-line
 * code or a lower-priority signal handler.
 * 
 *
 * Change History : 
 *
 * $Log: asyncScreenObj.h,v $
 * Revision 1.1  2006-10-16 22:45:26  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbCmdList.h"

class asyncScreenObject_t {
public:
   asyncScreenObject_t( fbCommandList_t &cmdList );
   virtual ~asyncScreenObject_t();

   // This function signals completion of on-screen update
   //
   // It may be over-ridden by derived classes to copy the 
   // command-list state to the on-screen state
   //
   virtual void executed();

   // This function must be provided by derived classes
   // to actually fix up the command list entry or entries
   virtual void updateCommandList() = 0 ;

   // This function should be called by updateCommandList() and 
   // the command list should only be updated if it returns false.
   bool valueBeingSet(void) const ;

   // Routines to set the desired state should call these
   // routines at the beginning and end of update to prevent
   // the updateCommandList() routine from updating the data
   void setValueStart();
   void setValueEnd();

protected:
   fbCommandList_t &cmdList_ ;
   unsigned         setValueFlag_ ;
};

#endif

