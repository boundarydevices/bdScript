#ifndef __JSFLASH_H__
#define __JSFLASH_H__ "$Id: jsFlash.h,v 1.1 2003-08-03 19:27:56 ericn Exp $"

/*
 * jsFlash.h
 *
 * This header file declares the initialization routine
 * for the flashMovie class and function.
 *
 * When called as a function, flashMovie() will download
 * a flash file synchronously and try parse the headers. 
 * (See jsCurl.h for right-hand object specs).
 *
 * When used as a constructor, flashMovie() queues a download
 * request, and the application can use the onLoad and onLoadError
 * properties to specify code to execute upon completion.
 * 
 * Properties of flashMovie objects include:
 *
 *    isLoaded    - true if loaded, false until then
 *
 * Methods of flashMovie objects include:
 *
 *    play( { onComplete:"handler", onCancel="handler" } );
 *    release();     - releases the cache file handle
 *
 * Change History : 
 *
 * $Log: jsFlash.h,v $
 * Revision 1.1  2003-08-03 19:27:56  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "js/jsapi.h"

bool initJSFlash( JSContext *cx, JSObject *glob );

#endif

