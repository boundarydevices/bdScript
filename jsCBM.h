#ifndef __JSCBM_H__
#define __JSCBM_H__ "$Id: jsCBM.h,v 1.3 2004-05-05 03:18:08 ericn Exp $"

/*
 * jsCBM.h
 *
 * This header file declares the initialization routine for the
 * CBM BD2-1220 receipt printer.
 * 
 * Javascript usage:
 *
 *    printer.print( image ); // image must be an alpha-map
 *
 * Change History : 
 *
 * $Log: jsCBM.h,v $
 * Revision 1.3  2004-05-05 03:18:08  ericn
 * -public CBMPrinterFixup for use with jsPrinter
 *
 * Revision 1.2  2003/05/10 19:14:21  ericn
 * -added closeCBM routine
 *
 * Revision 1.1  2003/05/09 04:28:12  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSCBM( JSContext *cx, JSObject *glob );

bool closeCBM( JSContext *cx, JSObject *glob );

extern JSFunctionSpec cbm_methods[10];

void CBMPrinterFixup( JSContext *cx, 
                      JSObject  *obj );


#endif

