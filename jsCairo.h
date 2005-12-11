#ifndef __JSCAIRO_H__
#define __JSCAIRO_H__ "$Id: jsCairo.h,v 1.1 2005-12-11 16:01:55 ericn Exp $"

/*
 * jsCairo.h
 *
 * This header file declares the initialization
 * routines for the cairo class and associated 
 * classes. 
 *
 * The cairo class and global object are primarily
 * used as a namespace for constants. 
 *
 *
 * Change History : 
 *
 * $Log: jsCairo.h,v $
 * Revision 1.1  2005-12-11 16:01:55  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */

#include "js/jsapi.h"

bool initJSCairo( JSContext *cx, JSObject *glob );
void shutdownJSCairo();

extern JSClass jsCairoClass_ ;
extern JSClass jsCairoSurfaceClass_ ;
extern JSClass jsCairoContextClass_ ;
extern JSClass jsCairoPatternClass_ ;

#include <cairo.h>

inline cairo_surface_t *surfObject( JSContext *cx, 
                                    JSObject *obj )
{
   return (cairo_surface_t *)JS_GetInstancePrivate( cx, obj, &jsCairoSurfaceClass_, NULL );
}

inline cairo_t *ctxObject( JSContext *cx, 
                           JSObject *obj )
{
   return (cairo_t *)JS_GetInstancePrivate( cx, obj, &jsCairoContextClass_, NULL );
}

inline cairo_pattern_t *patObject
   ( JSContext *cx, 
     JSObject *obj )
{
   return (cairo_pattern_t *)JS_GetInstancePrivate( cx, obj, &jsCairoPatternClass_, NULL );
}

unsigned getCairoStride( 
   cairo_format_t format,
   unsigned width );

extern cairo_user_data_key_t const cairoPixelKey_ ;

#endif

