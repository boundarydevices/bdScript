#ifndef __GPIOPOLL_H__
#define __GPIOPOLL_H__ "$Id: gpioPoll.h,v 1.1 2003-10-05 19:15:44 ericn Exp $"

/*
 * gpioPoll.h
 *
 * This header file declares the gpioPoll_t poll-able
 * input handler, which is used to allow a single thread to
 * process input for GPIO pins.
 *
 *
 * Change History : 
 *
 * $Log: gpioPoll.h,v $
 * Revision 1.1  2003-10-05 19:15:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "pollHandler.h"

class gpioPoll_t : public pollHandler_t {
public:
   gpioPoll_t( pollHandlerSet_t &set,
               char const       *devName );      // "/dev/Feedback" et al
   ~gpioPoll_t( void );

   bool isOpen( void ) const { return 0 <= getFd(); }
   
   //
   // Override this to perform processing of any transition. 
   // Note that the state is set prior to this call
   //
   // the default calls the onHigh() and onLow() methods
   virtual void onTransition( void );

   virtual void onHigh( void );
   virtual void onLow( void );

   bool         isHigh( void ){ return isHigh_ ; }

   virtual void onDataAvail( void );

protected:
   char    *const devName_ ;
   bool           isHigh_ ;
};

#endif

