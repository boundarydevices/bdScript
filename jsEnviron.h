#ifndef __JSENVIRON_H__
#define __JSENVIRON_H__ "$Id: jsEnviron.h,v 1.1 2002-12-12 15:41:34 ericn Exp $"

/*
 * jsEnviron.h
 *
 * This header file declares the initialization routine
 * for the getenv() and setenv() routines, which allow
 * environment variables to be read and set from JavaScript.
 *
 *    getenv( 'varname' );
 *    setenv( 'varname', value );
 *
 * Change History : 
 *
 * $Log: jsEnviron.h,v $
 * Revision 1.1  2002-12-12 15:41:34  ericn
 * -added environment routines
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSEnv( JSContext *cx, JSObject *glob );

#endif

