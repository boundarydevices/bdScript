#ifndef __SM501CURSOR_H__
#define __SM501CURSOR_H__ "$Id: sm501Cursor.h,v 1.1 2008-10-29 15:30:47 ericn Exp $"

/*
 * sm501Cursor.h
 *
 * This header file declares the sm501Cursor_t class, which is used 
 * to display a hardware cursor of the specified color, width, and 
 * height at the specified screen position.
 *
 * Change History : 
 *
 * $Log: sm501Cursor.h,v $
 * Revision 1.1  2008-10-29 15:30:47  ericn
 * [sm501 cursor] Added full mouse cursor support on SM-501
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "image.h"

class sm501Cursor_t {
public:
	sm501Cursor_t( image_t const &img );
	~sm501Cursor_t( void );

	void setPos( unsigned x, unsigned y );
	void getPos( unsigned &x, unsigned &y );

	void activate(void);
	void deactivate(void);

	static sm501Cursor_t *get(void);
	static void destroy();

private:
        sm501Cursor_t( sm501Cursor_t const & );
};

#endif

