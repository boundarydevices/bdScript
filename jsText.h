#ifndef __JSTEXT_H__
#define __JSTEXT_H__ "$Id: jsText.h,v 1.3 2002-11-02 04:10:59 ericn Exp $"

/*
 * jsText.h
 *
 * This header file declares the initialization routine
 * for the text output routines and freeType classes:
 *
 *    jsTextAt( fontData( string ), 
 *              pointSize, 
 *              x, y, 
 *              string, 
 *              [fgColor=(RGB)( > 0xFFFFFF == default white) ], 
 *              [bgColor=(RGB)( > 0xFFFFFF == default black) ] );
 *
 *    object font( { url:"something", 
 *                   [useCache:true], 
 *                   [onLoad="doSomething();"], 
 *                   [onLoadError="somethingElse();"] } );
 *
 *
 * Change History : 
 *
 * $Log: jsText.h,v $
 * Revision 1.3  2002-11-02 04:10:59  ericn
 * -added font comments
 *
 * Revision 1.2  2002/10/20 16:29:44  ericn
 * -fixed comment
 *
 * Revision 1.1  2002/10/18 01:18:25  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "js/jsapi.h"

bool initJSText( JSContext *cx, JSObject *glob );

#endif

