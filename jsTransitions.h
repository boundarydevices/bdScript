#ifndef __JSTRANSITIONS_H__
#define __JSTRANSITIONS_H__ "$Id: jsTransitions.h,v 1.1 2003-03-12 02:57:42 ericn Exp $"

/*
 * jsTransitions.h
 *
 * This header file declares the initialization 
 * routine for image transitions. All transitions
 * are declared in the form:
 *
 *    var transition = new fade( imageSrc, imageDest, ms, x, y );
 *
 * where imageSrc is replaced by imageDest over ms milliseconds.
 *
 * The application needs to call transition.tick()
 * periodically to cause the transition to occur.
 *
 * Transitions supported include:
 *
 *    fade()   - gradual alpha replacement of src with destination
 *
 * Note that only same-size images are currently supported.
 *
 * Change History : 
 *
 * $Log: jsTransitions.h,v $
 * Revision 1.1  2003-03-12 02:57:42  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSTransitions( JSContext *cx, JSObject *glob );

#endif

