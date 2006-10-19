#ifndef __FBCMDWAIT_H__
#define __FBCMDWAIT_H__ "$Id: fbCmdWait.h,v 1.2 2006-10-19 00:32:14 ericn Exp $"

/*
 * fbCmdWait.h
 *
 * This header file declares the fbWait_t command
 * class to wait for a certain SM-501 state.
 *
 * Refer to the documentation for the Status Test 
 * command in the SM-501 data book for details.
 *
 * Change History : 
 *
 * $Log: fbCmdWait.h,v $
 * Revision 1.2  2006-10-19 00:32:14  ericn
 * -retarget
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbCmdList.h"


//
// wait for 
//
class fbWait_t : public fbCommand_t {
public:
   fbWait_t( unsigned long bitsOfInterest,
             unsigned long values );
   virtual ~fbWait_t( void );

   virtual void const *data( void ) const ;
   virtual void retarget( void *data );

private:
   unsigned long data_[2];
   unsigned long *cmdPtr_ ;
};


#define WAITFORDRAWINGENGINE 0x00000001
#define DRAWINGENGINEIDLE    0x00000000
#define DRAWINGENGINEBUSY    0x00000001

#endif

