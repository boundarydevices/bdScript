#ifndef __JSMD5_H__
#define __JSMD5_H__ "$Id: jsMD5.h,v 1.1 2004-09-27 04:51:31 ericn Exp $"

/*
 * jsMD5.h
 *
 * This header file declares the initialization
 * routines for the Javascript md5 hash routines:
 *
 *    md5( 'string' );
 *    md5File( 'fileName' );
 *    md5Cram( '/dev/mtdblockx' );
 *
 * Each of these routines returns a 32-byte string
 * (16-bytes in Hex). The second and third versions
 * of the routine return false for failure.
 *
 * Change History : 
 *
 * $Log: jsMD5.h,v $
 * Revision 1.1  2004-09-27 04:51:31  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include "js/jsapi.h"

bool initJSMD5( JSContext *cx, JSObject *glob );

#endif

