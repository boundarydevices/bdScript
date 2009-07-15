#ifndef __JSSHA1_H__
#define __JSSHA1_H__
#include "js/jsapi.h"

/*
 * jsSHA1.h
 *
 * Usage is : 
 *
 *    var sha = sha1sum("code goes here");
 *
 * Copyright Boundary Devices, Inc. 2002
 */

bool initJSSHA1(JSContext *cx, JSObject *glob);

#endif
