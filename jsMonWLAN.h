#ifndef __JSMONWLAN_H__
#define __JSMONWLAN_H__ "$Id: jsMonWLAN.h,v 1.1 2003-08-20 02:54:02 ericn Exp $"

/*
 * jsMonWLAN.h
 *
 * This header file declares the initialization routine
 * for the WLAN-monitoring class monitorWLAN. 
 *
 * This class will generate call-backs to user-supplied
 * functions in response to the following events:
 *
 *    notconnected
 *    connected
 *    disconnected
 *    ap_change
 *    ap_outofrange
 *    ap_inrange
 *    assocfail
 *
 * Callbacks can be installed in one of two ways:
 * 
 * - by overriding the callback( event, device ) routine. 
 *
 *   This routine should expect the event and device name 
 *   as string parameters. This is most useful for detecting
 *   all wireless LAN events.
 *
 * - by installing callbacks with names as shown above. Each
 *   of these callbacks should expect the device name as a 
 *   parameter. For example:
 *
 *   var myMon = new monitorWLAN();
 *   myMon.disconnected = function( deviceName )
 *   {
 *       print( "WLAN device ", deviceName, " disconnected\n" );
 *   }
 *
 * Since this class is mostly useful for monitoring the status
 * of a wireless LAN device, a set of secondary routines is 
 * available in the wrapper module wlanMon.js. These simply
 * wrap the wlanctl-ng program in an easier-to-use interface.
 *
 * Change History : 
 *
 * $Log: jsMonWLAN.h,v $
 * Revision 1.1  2003-08-20 02:54:02  ericn
 * -added module jsMonWLAN
 *
 *
 *
 * Copyright Boundary Devices Inc. 2003
 */


#include "js/jsapi.h"

bool initMonitorWLAN( JSContext *cx, JSObject *glob );

#endif

