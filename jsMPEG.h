#ifndef __JSMPEG_H__
#define __JSMPEG_H__ "$Id: jsMPEG.h,v 1.1 2003-07-30 20:26:36 ericn Exp $"

/*
 * jsMPEG.h
 *
 * This header file declares the initialization routine for 
 * the mpegFile class and function.
 *
 * When called as a function, mpegFile() will download
 * an MPEG file synchronously, parse the headers and 
 * return an mpegFile object.
 *
 * When used as a constructor, mpegFile() queues a download
 * request, and the application can use the onLoad and 
 * onLoadError properties to specify code to execute upon 
 * completion.
 * 
 * Properties of mpegFile objects include:
 *
 *    isLoaded    - true if loaded, false until then
 *    data        - raw MPEG data
 *    duration    - total milliseconds of playback
 *    streamCount - number of MPEG streams (normally 2, audio & video)
 *    numFrames   - array with frame count by stream type [ 'audio' ], [ 'video' ], and [ 'total' ]
 *    width       - width of video output
 *    height      - height of video output
 *
 * Methods of mpegFile objects include:
 *
 *    play( { onComplete:"handler", onCancel="handler" } );
 *
 * Playback of mpeg files can be achieved by using the mp3Cancel()
 * routine as described in jsMP3.h
 *
 * Change History : 
 *
 * $Log: jsMPEG.h,v $
 * Revision 1.1  2003-07-30 20:26:36  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSMPEG( JSContext *cx, JSObject *glob );


#endif

