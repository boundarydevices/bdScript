#ifndef __JSKERNEL_H__
#define __JSKERNEL_H__ "$Id: jsKernel.h,v 1.1 2003-11-30 16:45:53 ericn Exp $"

/*
 * jsKernel.h
 *
 * This header file declares the kernel variable (and class), 
 * which is used to test and update the kernel boot partition.
 *
 * Methods include:
 *
 *    md5()             - returns an md5 hash of the kernel partition
 *    upgrade( string ) - writes the specified data to flash (careful!)
 *
 * and the fileSystem variable (and class), with a single method:
 *
 *    upgrade( string ) - unmounts root and writes the specified data 
 *                        to flash (careful!) - Power off after making
 *                        this call
 * 
 * Change History : 
 *
 * $Log: jsKernel.h,v $
 * Revision 1.1  2003-11-30 16:45:53  ericn
 * -kernel and JFFS2 upgrade support
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "js/jsapi.h"

bool initJSKernel( JSContext *cx, JSObject *glob );

#endif

