#ifndef __JSSHELL_H__
#define __JSSHELL_H__ "$Id: jsShell.h,v 1.1 2002-11-17 22:25:04 ericn Exp $"

/*
 * jsShell.h
 *
 * This header file declares the initialization routine
 * for a set of shell routines for use by Javascript programs:
 *
 *    shell()         - executes /bin/sh
 *
 * Change History : 
 *
 * $Log: jsShell.h,v $
 * Revision 1.1  2002-11-17 22:25:04  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSShell( JSContext *cx, JSObject *glob );


#endif

