#ifndef __JSUDP_H__
#define __JSUDP_H__ "$Id: jsUDP.h,v 1.1 2003-09-10 04:56:30 ericn Exp $"

/*
 * jsUDP.h
 *
 * This header file declares the initialization routines
 * for the Javascript UDP socket class.
 *
 * Usage is :
 *
 *    var udpSocket = udpSocket( {port:0xNNNN, onMsg:handler, onMsgObj:object } );
 *
 * All of the parameters are optional, although the onMsgObj
 * parameter is pretty useless without a handler. The handler is
 * called in the global scope by default, and accepts a single
 * parameter with the message string (could be binary).
 *
 * Methods are:
 *
 *    sendto( ip, port, string );
 *
 * Change History : 
 *
 * $Log: jsUDP.h,v $
 * Revision 1.1  2003-09-10 04:56:30  ericn
 * -Added UDP support
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSUDP( JSContext *cx, JSObject *glob );


#endif

