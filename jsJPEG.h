#ifndef __JSJPEG_H__
#define __JSJPEG_H__ "$Id: jsJPEG.h,v 1.1 2003-03-22 03:50:10 ericn Exp $"

/*
 * jsJPEG.h
 *
 * This header file declares the initialization routine
 * for the JPEG compression routine, imgToJPEG.
 *
 * imgToJPEG takes an image in normal RGB16 form (see 
 * jsImage.h for details) and produces a JPEG file image
 * as a string.
 *
 * It returns false if unsuccessful (i.e. if the parameters
 * are bad).
 *
 * This routine is typically called to produce a standard
 * format of an image for placement on a filesystem.
 *
 * Typical usage is:
 *
 *    var myImg = imgToJPEG( image );
 *    if( myImg )
 *    {
 *       // upload or save file here
 *    }
 *
 * Change History : 
 *
 * $Log: jsJPEG.h,v $
 * Revision 1.1  2003-03-22 03:50:10  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSJPEG( JSContext *cx, JSObject *glob );


#endif

