#ifndef __JSKERNEL_H__
#define __JSKERNEL_H__ "$Id: jsKernel.h,v 1.2 2003-12-01 04:55:03 ericn Exp $"

/*
 * jsKernel.h
 *
 * This header file declares the kernel variable (and class), 
 * which is used to test and update the kernel boot partition.
 *
 * Methods include:
 *
 *    md5()                - returns an md5 hash of the kernel partition
 *    upgrade( string,     - writes the specified data to flash (careful!)
 *           [ startX,
 *             endX,
 *             topY,
 *             height ] );
 *
 * and the fileSystem variable (and class), with a single method:
 *
 *    upgrade( string,     - unmounts root and writes the specified data 
 *           [ startX,
 *             endX,                !Power off after this call!
 *             topY,
 *             height ] );
 * 
 *
 * Note that both of the upgrade() routines accept optional startX,
 * endX, topY, and height parameters, which are used to display a 
 * horizontal progress bar (starts white, goes black during erase, 
 * green as the flash partition is programmed).
 * 
 * Change History : 
 *
 * $Log: jsKernel.h,v $
 * Revision 1.2  2003-12-01 04:55:03  ericn
 * -adde progress bar
 *
 * Revision 1.1  2003/11/30 16:45:53  ericn
 * -kernel and JFFS2 upgrade support
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "js/jsapi.h"

bool initJSKernel( JSContext *cx, JSObject *glob );

#endif

