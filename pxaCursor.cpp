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

pxaCursor_t::pxaCursor_t(unsigned int lmode) : cursor(lmode),
						mode(lmode)
{
	const char *cursorDev = getenv("CURSORDEV");

	/*
 	 * Set mode in pxa registers
 	 */
	setMode(lmode);
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
	close(fd_hc);
	if( this == instance_ )
		instance_ = 0 ;
}

void pxaCursor_t::setImage(image_t const &img)
{
	cursor.cursorFromFile(img);

	struct color16_info ci16;

	unsigned short *colors = cursor.getCursorColors();

	unsigned char *cursor_data = cursor.getCursorData();

	for(int idx = 0; idx < 4; idx++) {
		ci16.color = colors[idx];
		ci16.color_idx = idx;
		ioctl(fd_hc, PXA27X_16_BIT_COLOR, &ci16);
	}

	if(cursor.getCursorSize() > write(fd_hc, cursor_data,
					cursor.getCursorSize())) {
		printf("Unable to write complete cursor image\n");
		exit(-1);
	}
}

void pxaCursor_t::setMode(unsigned lmode)
{
	unsigned x, y;
	getPos(x, y);
	cursor.setMode(lmode);
	/*
 	 * Since we done have an explicit IOCTL to
 	 * set cursor mode we make use of setloc
 	 * IOCTL to set the mode
 	 */
	setPos(x, y);
}

unsigned int pxaCursor_t::getMode()
{
	return mode;
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
	/*
 	 * In case if activate is called before
 	 * setMode is called
 	 */
	setMode(mode);
	ioctl(fd_hc, PXA27X_CURSOR_ACTIVATE);
}

void pxaCursor_t::deactivate(void)
{
	ioctl(fd_hc, PXA27X_CURSOR_DEACTIVATE);
}

void pxaCursor_t::leave_cursor_atexit(void)
{
	ioctl(fd_hc, PXA27X_DONT_REMOVE_CURSOR_ATEXIT);
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


#ifdef STANDALONE
#include <stdio.h>
#include "fbDev.h"
#include <stdlib.h>
#include "imgFile.h"

static void print_usage(char *prog_name)
{
	printf("Usage: %s <-i img-file-name> [-m cursor-mode|help] [-x x-pos] [-y y-pos] [-h] <-d>\n\
	where\n\
		-i	-	cursor image file, necessary to enable cursor\n\
		-m	-	cursor mode number, or help to display supported modes (default mode 0)\n\
		-x	-	cursor x position (default 0)\n\
		-y	-	cursor y position (default 0)\n\
		-h	-	displays this usage\n\
		-d	-	disable cursor\n", prog_name);
}

int main(int argc, char **argv)
{
	int opt;
	char *img_file = NULL;
	int mode = 0;
	int x = 0;
	int y = 0;
	bool disable = false;

	if(argc == 1) { /* print usage */
		print_usage(argv[0]);
		return -1;
	}

	while ((opt = getopt(argc, argv, "i:m:x:y:hd")) != -1) {
		switch (opt) {
		case 'i':
			img_file = (char *) calloc(strlen(optarg)+1, 1);
			memcpy(img_file, optarg, strlen(optarg));
			break;
		case 'm':
			if(strcmp(optarg, "help") == 0) {
				printf("Supported modes:\n\
	0 - 32x32 2 Color and Transparency Mode\n\
	1 - 32x32 3 Color and transparency Mode\n\
	2 - 32x32 4 Color Mode\n\
	3 - 64x64 2 Color and Transparency Mode\n\
	4 - 64x64 3 Color and transparency Mode\n\
	5 - 64x64 4 Color Mode\n\
	6 - 128x128 2 Color Mode\n\
	7 - 128x128 1 Color and Transparency Mode\n");
				return 0;
			}
			mode = atoi(optarg);
			break;
		case 'x':
			x = atoi(optarg);
			break;
		case 'y':
			y = atoi(optarg);
			break;
		case 'h':
			print_usage(argv[0]);
			return 0;
		case 'd':
			disable = true;
			break;
		default:
			/* ignore non-options */
			print_usage(argv[0]);
			return -1;
		}
	}

	pxaCursor_t pcursor(mode);

	if(disable) {
		pcursor.deactivate();
		return 0;
	}

	if(img_file == NULL) { /* cannot enable cursor without an image */
		pcursor.deactivate();
		printf("Cursor cannot be enabled without an image.\n");
		print_usage(argv[0]);
		return -1;
	}

	pcursor.leave_cursor_atexit();
	fbDevice_t &fb = getFB();
	int maxx = fb.getWidth() - 1;
	int maxy = fb.getHeight() - 1;

	if(x > maxx)
		x = maxx;
	if(y > maxy)
		y = maxy;

	image_t image;
	if(imageFromFile(img_file, image)) {
		pcursor.setImage(image);
		pcursor.setPos(x,y);
	}
	else
		printf("unable to load image file %s %d\n", img_file, strlen(img_file));

	return 0;
}
#endif
