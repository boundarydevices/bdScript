#ifndef __JSPING_H__
#define __JSPING_H__ "$Id: jsPing.h,v 1.1 2003-08-23 02:50:26 ericn Exp $"

/*
 * jsPing.h
 *
 * This header file declares the initialization
 * routine for the pinger() Javascript class.
 *
 * Usage of the pinger() class is as follows:
 *
 *       var myPinger = new pinger( "192.168.0.255", myResponseFunc );
 *
 *       and when done, use pinger.cancel() to stop pinging.
 *
 * the response function should have the following 
 * structure:
 *
 *    function pingResponse( ipAddr )
 *    {
 *    }
 *
 * It should accept an IP address string as a parameter. Note
 * that it runs in the context of the pinger, so it can use 'this'
 * to cancel additional ping responses.
 *
 *
 * Change History : 
 *
 * $Log: jsPing.h,v $
 * Revision 1.1  2003-08-23 02:50:26  ericn
 * -added Javascript ping support
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "js/jsapi.h"

bool initPing( JSContext *cx, JSObject *glob );

#endif

