#ifndef __JSMP3_H__
#define __JSMP3_H__ "$Id: jsMP3.h,v 1.6 2002-11-30 00:31:39 ericn Exp $"

/*
 * jsMP3.h
 *
 * This header file declares the initialization routine
 * for the mp3File class and function.
 *
 * When called as a function, mp3File() will download
 * an MP3 file synchronously and try parse the headers. 
 * (See jsCurl.h for right-hand object specs).
 *
 * When used as a constructor, mp3File() queues a download
 * request, and the application can use the onLoad and onLoadError
 * properties to specify code to execute upon completion.
 * 
 * Properties of mp3File objects include:
 *
 *    isLoaded    - true if loaded, false until then
 *    data        - raw MP3 data
 *    frameCount  - number of MP3 frames
 *    sampleRate  - output playback rate
 *    numChannels - number of output channels
 *
 * Methods of mp3File objects include:
 *
 *    play( { onComplete:"handler", onCancel="handler" } );
 *
 * It also defines the following functions:
 *
 *    mp3Cancel(); - stops playback of the current file, and clears any queued files
 * 
 * Change History : 
 *
 * $Log: jsMP3.h,v $
 * Revision 1.6  2002-11-30 00:31:39  ericn
 * -implemented in terms of ccActiveURL module
 *
 * Revision 1.5  2002/11/14 14:24:17  ericn
 * -added mp3Cancel() routine
 *
 * Revision 1.4  2002/11/07 02:14:18  ericn
 * -updated comments
 *
 * Revision 1.3  2002/10/25 14:19:08  ericn
 * -added mp3Skip() and mp3Count() routines
 *
 * Revision 1.2  2002/10/25 02:52:46  ericn
 * -added mpWait() routine
 *
 * Revision 1.1  2002/10/24 13:19:06  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSMP3( JSContext *cx, JSObject *glob );

#endif

