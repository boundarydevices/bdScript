#ifndef __JSCURSOR_H__
#define __JSCURSOR_H__ "$Id: jsCursor.h,v 1.1 2007-07-07 00:25:52 ericn Exp $"

/*
 * jsCursor.h
 *
 * This header file declares a set of routines to manipulate
 * the on-screen cursor of an SM_501 graphics controller.
 *
 * 	setCursor(image)
 *	enableCursor()
 *	clearCursor()
 *	trackCursor(inputFileName);
 *
 * Change History : 
 *
 * $Log: jsCursor.h,v $
 * Revision 1.1  2007-07-07 00:25:52  ericn
 * -[bdScript] added mouse cursor and input handling
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#if KERNEL_FB_SM501 == 1

	#include "js/jsapi.h"
	
	bool initJSCursor( JSContext *cx, JSObject *glob );
	
#endif

#endif

