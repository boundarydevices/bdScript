#ifndef __JSCURL_H__
#define __JSCURL_H__ "$Id: jsCurl.h,v 1.3 2002-10-13 14:36:11 ericn Exp $"

/*
 * jsCurl.h
 *
 * This header file declares the Javascript wrappers for
 * curl objects.
 *                                                                                     static              static
 *    Object            constructor             methods           properties           methods           properties
 *    curlFile          curlFile( url )                           isOpen
 *                                                                size
 *                                                                data
 *                                                                url
 *                                                                httpCode
 *                                                                fileTime
 *                                                                mimeType 
 *
 * and curl routines:
 *
 *    curlGet()      - grabs a URL using http GET or POST and returns the content
 *
 * curlGet these takes one or more parameters:
 *    url         - relative or absolute (http://) URL
 *    useCache    - boolean indicating whether cached copy should be used
 *    variables   - Any variables to be posted. Variable
 *                  names will be used as form variable names
 *                  for POSTs. No GET parameter name support is available
 * 
 * and each returns an object (array) with the following
 * members:
 *    worked   - boolean : meaning is obvious
 *    status   - HTTP status
 *    mimeType - mime type of retrieved file
 *    content  - String with the content of the URL
 *
 * Change History : 
 *
 * $Log: jsCurl.h,v $
 * Revision 1.3  2002-10-13 14:36:11  ericn
 * -removed curlPost(), fleshed out variable handling
 *
 * Revision 1.2  2002/10/13 13:50:55  ericn
 * -merged curlGet() and curlPost() with curlFile object
 *
 * Revision 1.1  2002/09/29 17:36:23  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSCurl( JSContext *cx, JSObject *glob );

#endif

