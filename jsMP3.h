#ifndef __JSMP3_H__
#define __JSMP3_H__ "$Id: jsMP3.h,v 1.3 2002-10-25 14:19:08 ericn Exp $"

/*
 * jsMP3.h
 *
 * This header file declares the initialization routine
 * for the MP3 output routines:
 *
 *    mp3Play( url );      // queue playback of url
 *    mp3Wait();           // wait for queue to flush
 *    mp3Skip();           // skip to end of currently running file
 *    mp3Count();          // returns count of queued files
 *
 * Note that mp3Play() is an asynchronous call. If it 
 * returns true, all that is indicated is that the url 
 * was downloaded and playback queued.
 *
 *
 * Change History : 
 *
 * $Log: jsMP3.h,v $
 * Revision 1.3  2002-10-25 14:19:08  ericn
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

