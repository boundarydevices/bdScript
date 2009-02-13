#ifndef __CURSOR_FILE_H__
#define __CURSOR_FILE_H__

/*
 * cursorFile.h
 *
 * This header file declares the cursorFromFile() 
 * routine, which tries to load an cursor from file.
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "image.h"
#include "video/pxa27xfb.h"

#define MAX_WIDTH	128
#define MAX_HEIGHT	128

#define COLOR_1		0
/*
 * This is also used to indicate transparency in 1 color and transparency
 * mode
 */
#define COLOR_2		1
/* 
 * This is also used to indicate Transparency in 2 color and transparency
 * mode
 */
#define COLOR_3		2
/* 
 * This is also used to Transparency but inverted in 2 color and
 * transparency mode and to indicate transparency in 3 color and
 * transparency mode
 */
#define	COLOR_4		3

#define INC_SIZE	4

class cursor_t {
public:
   cursor_t(unsigned int lmode = 0);

   ~cursor_t();

   bool cursorFromFile(image_t const &image);

   unsigned char *getCursorData();

   void setMode(unsigned lmode);

   unsigned getMode();

   unsigned short getCursorSize();
private:
   void update_colors(unsigned short* &colors_count, unsigned short &c_idx,
                      unsigned short &c_size, unsigned short pc);

   void find_4colors(unsigned short *colors_count, unsigned short &c_idx);
   unsigned char *cursor_data;
   unsigned short *colors;
   unsigned int mode;
   unsigned short width;
   unsigned short height;
   unsigned short cursor_size;
};


#endif

