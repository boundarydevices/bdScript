#ifndef __INPUTDEVS_H__
#define __INPUTDEVS_H__ "$Id: inputDevs.h,v 1.1 2008-09-22 19:07:52 ericn Exp $"

/*
 * inputDevs.h
 *
 * This header file declares the inputDevs_t class, which 
 * reads /proc/bus/input/devices to produce a set of 
 * keyboard, touch screen, and mouse devices for use in
 * creating handlers.
 *
 * Change History : 
 *
 * $Log: inputDevs.h,v $
 * Revision 1.1  2008-09-22 19:07:52  ericn
 * [touch] Use input devices for mouse, touch screen
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include <assert.h>

class inputDevs_t {
public:
	inputDevs_t();
	~inputDevs_t();
	
	enum type_e {
		MOUSE,
		TOUCHSCREEN,
		KEYBOARD
	};

	inline unsigned count() const { return count_ ; }
	inline void getInfo(unsigned idx,type_e &type, unsigned &eventIdx);

        bool findFirst( type_e type, unsigned &evIdx );

	static char const *typeName(type_e type);
private:
	void addDev( unsigned evidx, unsigned events );

	struct info_t {
		type_e 	 type ;
		unsigned evidx ;
	};

	unsigned 	count_ ;
        struct info_t  *info_ ;
};


void inputDevs_t::getInfo(unsigned idx,type_e &type, unsigned &eventIdx)
{
	assert(idx<count_);
        struct info_t *info = info_ + idx ;
	type = info->type ;
	eventIdx = info->evidx ;
}

#endif

