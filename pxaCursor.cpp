/*
 * Module pxaCursor.cpp
 *
 * This module defines the methods of the pxaCursor_t
 * class as declared in pxaCursor.h
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "pxaCursor.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "fbDev.h"
#include "cursorFile.h"

static pxaCursor_t *instance_ = 0 ;

pxaCursor_t::pxaCursor_t(unsigned int lmode) : cursor(lmode)
{
	const char *cursorDev = getenv("CURSORDEV");

	if(0 == cursorDev)
		cursorDev = "/dev/fb_cursor";

	fd_hc = open(cursorDev, O_WRONLY);

	if(fd_hc == -1) {
		printf("Hardware cursor device does not exist\n");
		exit(-1);
	}
}

pxaCursor_t::~pxaCursor_t()
{
        deactivate();
	close(fd_hc);
	if( this == instance_ )
		instance_ = 0 ;
}

void pxaCursor_t::setImage(image_t const &img)
{
	cursor.cursorFromFile(img);

	struct color16_info ci16;

	for(int idx = 0; idx < 4; idx++) {
		ci16.color = cursor.colors[idx];
		ci16.color_idx = idx;
		ioctl(fd_hc, PXA27X_16_BIT_COLOR, &ci16);
	}

	if(cursor.getCursorSize() > write(fd_hc, cursor.cursor_data,
					cursor.getCursorSize())) {
		printf("Unable to write complete cursor image\n");
		exit(-1);
	}
}

void pxaCursor_t::setMode(unsigned lmode)
{
	cursor.setMode(lmode);
}

void pxaCursor_t::setPos(unsigned x, unsigned y)
{
	struct cursorfb_info loc;
	loc.x_loc = x;
	loc.y_loc = y;
	loc.mode = cursor.getMode();
	ioctl(fd_hc, PXA27X_CURSOR_SETLOC, &loc);
}

void pxaCursor_t::getPos(unsigned &x, unsigned &y)
{
	struct cursorfb_info loc;
	x = y = 0;
	int res = ioctl(fd_hc, PXA27X_CURSOR_GETLOC, &loc);
	if( 0 == res ){
		x = loc.x_loc;
		y = loc.y_loc;
	}
}

void pxaCursor_t::activate()
{
	ioctl(fd_hc, PXA27X_CURSOR_ACTIVATE);
}

void pxaCursor_t::deactivate(void)
{
	ioctl(fd_hc, PXA27X_CURSOR_DEACTIVATE);
}

pxaCursor_t *pxaCursor_t::get(void)
{
	return instance_ ;
}

void pxaCursor_t::destroy()
{
	if( instance_ )
		instance_->deactivate();
}


