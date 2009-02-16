/*
 * Module cursorFile.cpp
 *
 * This module defines the cursorFromFile() routine
 * as declared in cursorFile.h
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "cursorFile.h"
// #define DEBUGPRINT
#include "debugPrint.h"

static struct cursorfb_mode modes[] = {
	{32, 32, 2, 2, 3},   /* 2 color and transparency */
	{32, 32, 2, 3, 4},   /* 3 color and transparency */
	{32, 32, 2, 4, -1},   /* 4 color */
	{64, 64, 2, 2, 3},   /* 2 color and transparency */
	{64, 64, 2, 3, 4},   /* 3 color and transparency */
	{64, 64, 2, 4, -1},   /* 4 color */
	{128, 128, 1, 2, -1}, /* 2 color */
	{128, 128, 1, 1, 1}  /* 1 color and transparency */
};

cursor_t::cursor_t(unsigned int lmode) : cursor_data(0),
					colors(0),
					mode(lmode)
{
	height = modes[mode].yres;
	width = modes[mode].xres;
	cursor_size = (height * width * modes[mode].bpp)/8;
}

unsigned char *cursor_t::getCursorData()
{
	return cursor_data;
}

unsigned short *cursor_t::getCursorColors()
{
	return colors;
}

void cursor_t::setMode(unsigned lmode)
{
	mode = lmode;
	height = modes[mode].yres;
	width = modes[mode].xres;
	cursor_size = (height * width * modes[mode].bpp)/8;
}

unsigned cursor_t::getMode()
{
	return mode;
}

unsigned short cursor_t::getCursorSize()
{
	return cursor_size;
}

cursor_t::~cursor_t()
{
	if(cursor_data)
		free(cursor_data);
	if(colors)
		free(colors);
}

void cursor_t::update_colors(unsigned short* &colors_count,
				unsigned short &c_idx,
				unsigned short &c_size,
				unsigned short pc)
{
	unsigned short idx = 0;

	for(idx = 0; idx < c_idx; idx++) {
		if(colors[idx] == pc)
			break;
	}

	if(idx >= c_idx) {
		if(idx == c_size) {
			c_size += INC_SIZE;
			colors = (unsigned short *) realloc(colors,
					c_size*sizeof(unsigned short));
			colors_count = (unsigned short *) realloc(colors_count,
					c_size*sizeof(unsigned short));
		}
		colors[c_idx++] = pc;
		colors_count[idx] = 0;
	}

	colors_count[idx]++;
}

/*
 * finds the 4 colors that occur the maximum number of times
 * in pixel map. when this function returns array elements
 * 0, 1, 2, 3 of colors array will contain the max colors
 * with colors[0] being the color that occurs a lot more
 * than colors[3] in the pixel map
 */
void cursor_t::find_4colors(unsigned short *colors_count,
				unsigned short &c_idx)
{
	unsigned short max[4] = { 0, 0, 0, 0};
	unsigned short max_idx[4] = {-1, -1, -1, -1};
	unsigned short max_colors[4] = {0, 0, 0, 0};
	unsigned short idx = 0;

	for(idx = 0; idx < c_idx; idx++) {
		if(max[0] < colors_count[idx]) {
			max[3] = max[2]; max_idx[3] = max_idx[2];
			max[2] = max[1]; max_idx[2] = max_idx[1];
			max[1] = max[0]; max_idx[1] = max_idx[0];
			max[0] = colors_count[idx]; max_idx[0] = idx;
		}
		else if(max[1] < colors_count[idx]) {
			max[3] = max[2]; max_idx[3] = max_idx[2];
			max[2] = max[1]; max_idx[2] = max_idx[1];
			max[1] = colors_count[idx]; max_idx[1] = idx;
		}
		else if(max[2] < colors_count[idx]) {
			max[3] = max[2]; max_idx[3] = max_idx[2];
			max[2] = colors_count[idx]; max_idx[2] = idx;
		}
		else if(max[3] < colors_count[idx]) {
			max[3] = colors_count[idx];
			max_idx[3] = idx;
		}
	}

	for(idx = 0; idx < 4; idx++)
		if(max_idx[idx] < c_idx)
			max_colors[idx] = colors[max_idx[idx]];

	for(idx = 0; idx < 4; idx++)
		colors[idx] = max_colors[idx];
}

#define SET_COLOR(ROW,PIXEL,BPP,COLOR) ROW[PIXEL/(8/BPP)]|=(COLOR<<((PIXEL%(8/BPP))*BPP))

bool cursor_t::cursorFromFile(image_t const &image)
{
	unsigned short        cc_size = 4;
	unsigned short        c_idx = 0;
	unsigned short       *colors_count = (unsigned short *)
					calloc (cc_size,
						sizeof(unsigned short));
	unsigned short        row = 0;
	unsigned short        column = 0;
	unsigned short const *pm;
	unsigned char const  *alp;
	unsigned short        col_bytes;

	colors = (unsigned short *) calloc (cc_size, sizeof(unsigned short));

	if(height > image.height_)
		height = image.height_;

	if(width > image.width_)
		width = image.width_;

	cursor_size = (height * width * modes[mode].bpp)/8;
	cursor_data = (unsigned char *) calloc(cursor_size, sizeof(unsigned short));
	pm = (unsigned short *) image.pixData_;
	alp = (unsigned char *) image.alpha_;
	col_bytes = (width * modes[mode].bpp)/8;

	/* 
	 * use alpha only if mode supports transparency and calculate
	 * color occurance.
	 */
	for(row = 0; row < height; row++) {
		unsigned char *row_data = cursor_data + (row * col_bytes);
		for(column = 0; column < width; column++) {
			if(alp && alp[row * height + column] <= 128 && modes[mode].transparency != -1)
				SET_COLOR(row_data,column,modes[mode].bpp,modes[mode].transparency);
			else
				update_colors(colors_count, c_idx, cc_size, pm[row*height+column]);
		}
	}

	find_4colors(colors_count, c_idx);
	free(colors_count);

	for(row = 0; row < height; row++) {
		unsigned char *row_data = cursor_data + (row * col_bytes);
		for(column = 0; column < width; column++) {
			unsigned short idx = 0;
			unsigned short chosen = 0;
			unsigned short min = 65535;
			for(idx = 0; idx < modes[mode].color_count; idx++) {
				unsigned short diff = (unsigned short)
						abs(pm[row*height+column]
							- colors[idx]);
				if(min > diff) {
					min = diff;
					chosen = idx;
				}
			}
			if(!alp || alp[row * height + column] > 128)
				SET_COLOR(row_data,column,modes[mode].bpp,chosen);
		}
	}

	return true;
}

#ifdef STANDALONE
#include "imgFile.h"

int main(int argc, char const * const argv[])
{
	if(2 <= argc) {
		image_t        image;
		int            mode;

		mode = DEFAULT_CURSOR_MODE;
		if( 3 <= argc )
			mode = strtoul( argv[2], 0, 0 );

		if(imageFromFile(argv[1], image))
			perror(argv[1]);

		cursor_t cursor(mode);
		if(cursor.cursorFromFile(image))
			perror(argv[1]);

		if(4 <= argc) {
			int fd = open(argv[3], O_WRONLY);

			if(-1 == fd) {
				printf("Unable to open cursor file to write image\n");
				return -1;
			}

			if(cursor.getCursorSize() > write(fd, cursor.getCursorData(), cursor.getCursorSize())) {
				printf("Unable to write complete cursor image\n");
				return -1;
			}
			close(fd);
		}
	}
	else
		fprintf(stderr, "Usage: cursorFile fileName [mode [convertedImgFile]]\n");

	return 0;
}   
#endif
