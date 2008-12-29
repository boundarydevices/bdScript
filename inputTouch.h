#ifndef __INPUTTOUCH_H__
#define __INPUTTOUCH_H__ "$Id: inputTouch.h,v 1.2 2008-12-29 17:49:48 ericn Exp $"

/*
 * inputTouch.h
 *
 * This header file declares the inputTouch_t class, which is
 * an input poll handler that handles touch-screen input.
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "inputPoll.h"
#include "touchCalibrate.h"
#include "rollingMedian.h"

#ifdef __DAVINCIFB__
#include "fbDevices.h"
#else
#include "fbDev.h"
#endif

class inputTouchScreen_t : public inputPoll_t {
public:
   // override this to handle keystrokes, button presses and the like
   inputTouchScreen_t( pollHandlerSet_t &set,
                       char const       *devName );
   virtual ~inputTouchScreen_t( void ){}

   inline bool haveCalibration( void ) const { return !raw_ ; }

   // called with calibrated point
   virtual void onTouch( unsigned x,
                         unsigned y );
   virtual void onRelease( void );

   // implementation
   virtual void onData( struct input_event const &event );

   inline bool isRaw(void) const { return raw_ ; }

   void setCooked(void);
   void setRaw(void);

protected:
   rollingMedian_t medianX_ ;
   rollingMedian_t medianY_ ;
   bool            raw_ ;
   int             i_ ;
   int             j_ ;
   unsigned        mask_ ; // which have we received (I or J)
   bool            isDown_ ;
   bool            wasDown_ ;
   unsigned        prevX_ ;
   unsigned        prevY_ ;
   unsigned        exitMS_ ;
   long long       lastTouch_ ;
};

#endif

