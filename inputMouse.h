#ifndef __INPUTMOUSE_H__
#define __INPUTMOUSE_H__ "$Id: inputMouse.h,v 1.1 2008-09-22 19:07:52 ericn Exp $"

/*
 * inputMouse.h
 *
 * This header file declares the input handler for mice (inputMouse_t).
 *
 * Copyright Boundary Devices, Inc. 2008
 */
#include "inputPoll.h"
#include "fbDev.h"
#include "flashplayer.h"

class inputMouse_t : public inputPoll_t {
public:
   // override this to handle keystrokes, button presses and the like
   inputMouse_t( pollHandlerSet_t &set,
                 char const       *devName );
   virtual ~inputMouse_t( void ){}

   inline void attach( FlashPlayer *player ){ player_ = player ; }

   // called with calibrated point
   virtual void onTouch( void );
   virtual void onRelease( void );

   // implementation
   virtual void onData( struct input_event const &event );

protected:
   FlashPlayer    *player_ ;
   int             x_ ;
   int             y_;
   bool            isDown_ ;
   unsigned        exitMS_ ;
   long long       lastTouch_ ;
};

#endif

