#ifndef __JSSNIFFWLAN_H__
#define __JSSNIFFWLAN_H__ "$Id: jsSniffWLAN.h,v 1.1 2003-08-12 01:20:38 ericn Exp $"

/*
 * jsSniffWLAN.h
 *
 * This header file declares the initialization
 * routine for the Javascript WLAN-sniffing routines:
 *
 *    - startSniffWLAN( { channel,           1..11
 *                        numSeconds,        > 0
 *                        onAP,              function
 *                        onComplete } );    function
 *
 *      This routine starts the sniffing process. 
 *      The function should expect an object as a parameter.
 *      The object will contain the following members:
 *
 *          SSID     : string, name of the wireless LAN
 *          AP       : string, mac address of the Access Point
 *          channel  : integer channel number (1-11)
 *          WEP      : boolean, true means link is encrypted
 *          signal   : signal level reported by the WLAN card
 *          noise    : noise level reported by the WLAN card
 *
 *      Once called for an Access Point, the sniffer will not
 *      report the same AP again until stopped and restarted.
 *
 *      Note that only one channel can be scanned at a time.
 *      Use the onComplete function to switch channels if
 *      desired.
 *
 *    - stopSniffWLAN()
 *
 *      This function stops the WLAN sniffing process.
 *
 * Change History : 
 *
 * $Log: jsSniffWLAN.h,v $
 * Revision 1.1  2003-08-12 01:20:38  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initSniffWLAN( JSContext *cx, JSObject *glob );

#endif

