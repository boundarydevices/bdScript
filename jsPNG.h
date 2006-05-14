#ifndef __JSPNG_H__
#define __JSPNG_H__ "$Id: jsPNG.h,v 1.1 2006-05-14 14:51:34 ericn Exp $"

/*
 * jsPNG.h
 *
 * This header file declares the initialization routine
 * for the PNG compression routine, imgToPNG.
 *
 * imgToPNG takes an image in normal RGB16 form (see 
 * jsImage.h for details) and produces a PNG file image
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
 *    var myImg = imgToPNG( image );
 *    if( myImg )
 *    {
 *       // upload or save file here
 *    }
 *
 * Change History : 
 *
 * $Log: jsPNG.h,v $
 * Revision 1.1  2006-05-14 14:51:34  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "js/jsapi.h"

bool initJSPNG( JSContext *cx, JSObject *glob );


#endif

