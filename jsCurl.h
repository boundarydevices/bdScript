#ifndef __JSCURL_H__
#define __JSCURL_H__ "$Id: jsCurl.h,v 1.4 2002-11-03 17:55:51 ericn Exp $"

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
 * Revision 1.4  2002-11-03 17:55:51  ericn
 * -modified to support synchronous gets and posts
 *
 * Revision 1.3  2002/10/13 14:36:11  ericn
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

