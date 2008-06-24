#ifndef __JSCURSOR_H__
#define __JSCURSOR_H__ "$Id: jsCursor.h,v 1.3 2008-06-24 23:32:27 ericn Exp $"

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
 * Revision 1.3  2008-06-24 23:32:27  ericn
 * [jsCursor] Add support for Davinci HW cursor
 *
 * Revision 1.2  2007/07/30 22:33:24  ericn
 * -Use KERNEL_FB_SM501, not NEON macro
 *
 * Revision 1.1  2007/07/07 00:25:52  ericn
 * -[bdScript] added mouse cursor and input handling
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#if (defined(KERNEL_FB_SM501) && (KERNEL_FB_SM501 == 1)) \
    || \
    (defined(KERNEL_FB_DAVINCI) && (KERNEL_FB_DAVINCI == 1))

	#include "js/jsapi.h"
	
	bool initJSCursor( JSContext *cx, JSObject *glob );
	
#endif

#endif

