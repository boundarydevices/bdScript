#ifndef __PXACURSOR_H__
#define __PXACURSOR_H__

/*
 * pxaCursor.h
 *
 * This header file declares the pxaCursor_t class, which is used 
 * to display a hardware cursor of the specified color, width, and 
 * height at the specified screen position.
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "image.h"
#include "cursorFile.h"

class pxaCursor_t {
public:
	pxaCursor_t(unsigned int lmode = DEFAULT_CURSOR_MODE);
	~pxaCursor_t();

        void setImage(image_t const &img);

	void setMode(unsigned int lmode);
	unsigned int getMode();

	void setPos(unsigned x, unsigned y);
	void getPos(unsigned &x, unsigned &y);

	void activate(void);
	void deactivate(void);

	void leave_cursor_atexit(void);

	static pxaCursor_t *get(void);
	static void destroy();
private:
        pxaCursor_t(pxaCursor_t const &);

	cursor_t cursor;
	int fd_hc; /* holds the file descriptor for hardware cursor */
	unsigned int mode;
};

#endif

