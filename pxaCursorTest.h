#ifndef __PXACURSORTEST_H__
#define __PXACURSORTEST_H__

/*
 * pxaCursorTest.h
 *
 * This header file declares the input handler for mice (pxaCursorTest_t).
 *
 * Copyright Boundary Devices, Inc. 2008
 */
#include "pxaCursor.h"
#include "imgFile.h"
#include "inputMouse.h"

class pxaCursorTest_t : protected inputMouse_t {
public:
   // override this to handle keystrokes, button presses and the like
	pxaCursorTest_t(pollHandlerSet_t &set,
			char const       *devName,
			unsigned          mode,
			char const       *cursorImg);

	virtual ~pxaCursorTest_t(){}

	// called with calibrated point
	virtual void onMove();

	virtual void onData(struct input_event const &event);

	virtual void enforceBounds();

protected:
	pxaCursor_t pcursor;
	int maxx;
	int minx;
	int maxy;
	int miny;

	int curx;
	int cury;
};

#endif

