#ifndef __JSHYPERLINK_H__
#define __JSHYPERLINK_H__ "$Id: jsHyperlink.h,v 1.2 2002-12-01 02:40:36 ericn Exp $"

/*
 * jsHyperlink.h
 *
 * This header file declares the initialization routines
 * for the hyperlinking Javascript routines:
 *
 *    goto( url )
 *
 *
 * Change History : 
 *
 * $Log: jsHyperlink.h,v $
 * Revision 1.2  2002-12-01 02:40:36  ericn
 * -made gotoCalled_ volatile
 *
 * Revision 1.1  2002/10/27 17:42:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"
#include <string>

extern bool volatile gotoCalled_ ;
extern std::string   gotoURL_ ;

bool initJSHyperlink( JSContext *cx, JSObject *glob );

#endif

