#ifndef __JSTCP_H__
#define __JSTCP_H__ "$Id: jsTCP.h,v 1.1 2002-12-15 20:02:58 ericn Exp $"

/*
 * jsTCP.h
 *
 * This header file declares the initialization routine
 * for the JavaScript TCP wrappers:
 *
 * tcpClient - object representing a TCP client socket
 *
 *    constructor takes an unnamed structure as an argument
 *    (to allow named parameters). Members include:
 * 
 *       serverIP   - (string) name or IP address of server (defaults to 'localhost')
 *       serverSock - (int) port # on server to connect()
 *       onLineIn   - (string) JavaScript code to execute on incoming lines
 *       onClose    - (string) JavaScript code to execute when(if) socket
 *                    is closed.
 *
 *    After construction (and attempted connection), the 
 *    tcpClient object has the following members:
 *
 *       isConnected - (bool) indicates that the socket connection
 *                     has been established (and is still connected).
 *       send(str)   - sends the string across the socket. Note that
 *                     the Javascript code should append a newline if
 *                     the send side of the socket is line oriented.
 *       close()     - closes the TCP connection
 *
 * Change History : 
 *
 * $Log: jsTCP.h,v $
 * Revision 1.1  2002-12-15 20:02:58  ericn
 * -added module jsTCP
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "js/jsapi.h"

bool initJSTCP( JSContext *cx, JSObject *glob );

#endif

