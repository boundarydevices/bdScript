#ifndef __JSPOPEN_H__
#define __JSPOPEN_H__ "$Id: jsPopen.h,v 1.1 2002-12-10 04:48:30 ericn Exp $"

/*
 * jsPopen.h
 *
 * This header file declares the initialization
 * routine for the popen interface for JavaScript.
 *
 *
 * Change History : 
 *
 * $Log: jsPopen.h,v $
 * Revision 1.1  2002-12-10 04:48:30  ericn
 * -added module jsPopen
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "js/jsapi.h"

bool initJSPopen( JSContext *cx, JSObject *glob );

#endif

