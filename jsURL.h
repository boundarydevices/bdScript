#ifndef __JSURL_H__
#define __JSURL_H__ "$Id: jsURL.h,v 1.3 2002-10-25 02:20:50 ericn Exp $"

/*
 * jsURL.h
 *
 * This header file declares the initialization routine
 * for the URL identification routines for JavaScript
 *
 *    currentURL()                - returns the currently active URL
 *    absoluteURL( relativeURL )  - returns the absolute URL for the
 *                                  specified relativeURL
 *    isRelativeURL( url )        - returns true if url is relative
 *
 * as well as the C++ interfaces:
 *
 *    currentURL()                - returns current url
 *    absoluteURL( relative )     - returns absolute URL for relative
 *    isRelativeURL( url )        - returns true if url is relative
 *    pushURL( url )              - activates a new url (relative or absolute)
 *    popURL()                    - deactivates the previous url
 * 
 * Change History : 
 *
 * $Log: jsURL.h,v $
 * Revision 1.3  2002-10-25 02:20:50  ericn
 * -moved non-Javascript stuff to relativeURL.cpp
 *
 * Revision 1.2  2002/10/24 13:15:58  ericn
 * -added pushURL() and popURL()
 *
 * Revision 1.1  2002/10/20 16:31:23  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "js/jsapi.h"

bool initJSURL( JSContext *cx, JSObject *glob );

#endif

