#ifndef __JSFLASHVAR_H__
#define __JSFLASHVAR_H__ "$Id: jsFlashVar.h,v 1.1 2004-02-07 12:14:49 ericn Exp $"

/*
 * jsFlashVar.h
 *
 * This header file declares the initialization routine for
 * the flashVar Javascript global and its 'get' and 'set' methods:
 *
 *    flashVar.get( 'varName' )        - returns string value (or '' if not found)
 *    flashVar.set( 'varName', value ) - sets value of flash variable 'varName' to value(as string)
 *
 * Change History : 
 *
 * $Log: jsFlashVar.h,v $
 * Revision 1.1  2004-02-07 12:14:49  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include "js/jsapi.h"

bool initJSFlashVar( JSContext *cx, JSObject *glob );

#endif

