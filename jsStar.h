#ifndef __JSSTAR_H__
#define __JSSTAR_H__ "$Id: jsStar.h,v 1.2 2004-05-08 23:54:45 ericn Exp $"

/*
 * jsStar.h
 *
 * This header file declares the interface method array
 * for the Star Micronics TUP-900 printer.
 *
 * Currently supported methods include:
 *
 *    imageToString( img );         - used by printer.print( img )
 *
 * Change History : 
 *
 * $Log: jsStar.h,v $
 * Revision 1.2  2004-05-08 23:54:45  ericn
 * -expanded star_methods for pageHeight() call
 *
 * Revision 1.1  2004/05/05 03:20:32  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include "js/jsapi.h"

extern JSFunctionSpec star_methods[3];

void starPrinterFixup( JSContext *cx, 
                       JSObject  *obj );

#endif

