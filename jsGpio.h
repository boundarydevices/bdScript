#ifndef __JSGPIO_H__
#define __JSGPIO_H__

/*
 * jsGpio.h
 *
 *    onFeedbackLow( "code goes here" );
 *    onFeedbackHigh( "code goes here" );
 *
 * and a couple of utility routines:
 *
 *    setAmber([0,1])
 *    setRed([0,1])
 *    setGreen([0,1])
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSGpio( JSContext *cx, JSObject *glob );
void shutdownGpio();

#endif

