#ifndef __JSMP3_H__
#define __JSMP3_H__ "$Id: jsMP3.h,v 1.1 2002-10-24 13:19:06 ericn Exp $"

/*
 * jsMP3.h
 *
 * This header file declares the initialization routine
 * for the MP3 output routines:
 *
 *    mp3Play( url );
 *
 * Change History : 
 *
 * $Log: jsMP3.h,v $
 * Revision 1.1  2002-10-24 13:19:06  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "js/jsapi.h"

bool initJSMP3( JSContext *cx, JSObject *glob );

#endif

