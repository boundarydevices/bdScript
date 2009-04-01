#ifndef __JSMOUSE_H__
#define __JSMOUSE_H__

/*
 * jsMouse.h
 *
 * This header file declares a set of routines to manipulate
 * the mouse.
 *
 *	enableCursor()
 *	disableCursor()
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#include "js/jsapi.h"
#include "fbDev.h"
#include "inputPoll.h"
#include "box.h"

class jsMouse_t : public inputPoll_t {
public:
   jsMouse_t( char const *devName );
   ~jsMouse_t( void );

   virtual void onData( struct input_event const &event );
   void press(void);
   void release(void);

   box_t       *curBox_ ;
   bool         down_ ;
   unsigned     x_ ;
   unsigned     y_ ;
   int          wheel_ ;
};

bool initJSMouse( JSContext *cx, JSObject *glob );
#endif

