#ifndef __JSCURL_H__
#define __JSCURL_H__ "$Id: jsCurl.h,v 1.1 2002-09-29 17:36:23 ericn Exp $"

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
 * Change History : 
 *
 * $Log: jsCurl.h,v $
 * Revision 1.1  2002-09-29 17:36:23  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

void initJSCurl( JSContext *cx, JSObject *glob );

#endif

