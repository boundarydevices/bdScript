#ifndef __JSURL_H__
#define __JSURL_H__ "$Id: jsURL.h,v 1.2 2002-10-24 13:15:58 ericn Exp $"

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
 * Revision 1.2  2002-10-24 13:15:58  ericn
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
#include <string>

//
// returns true and the url if established
//
bool currentURL( std::string &url );

//
// returns true and the absolute URL if an absolute URL 
// has been established or is present in relative
//
bool absoluteURL( std::string const &relative,   // input : arbitrary URL
                  std::string       &absolute ); // output : absolute URL

//
// returns true if the specified URL is relative (missing protocol and server)
//
bool isRelativeURL( std::string const &url );

//
// activates a new url (relative or absolute)
//
void pushURL( std::string const &url );

//
// deactivates the previous url
//
void popURL( void );

bool initJSURL( JSContext *cx, JSObject *glob );

#endif

