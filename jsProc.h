#ifndef __JSPROC_H__
#define __JSPROC_H__ "$Id: jsProc.h,v 1.1 2002-10-27 17:42:08 ericn Exp $"

/*
 * jsProc.h
 *
 * This header file declares the jsProc() routine,
 * which spawns a sub-process and returns a handle
 * to it.
 *
 * Note that internally, the handle is a pthread
 * handle to a watchdog thread.
 *
 * Change History : 
 *
 * $Log: jsProc.h,v $
 * Revision 1.1  2002-10-27 17:42:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSProc( JSContext *cx, JSObject *glob );

#endif

