#ifndef __JSHYPERLINK_H__
#define __JSHYPERLINK_H__ "$Id: jsHyperlink.h,v 1.4 2008-12-02 00:22:47 ericn Exp $"

/*
 * jsHyperlink.h
 *
 * This header file declares the initialization routines
 * for the hyperlinking Javascript routines:
 *
 *    goto( url )    - starts javascript interpreter on the 
 *                     specified URL
 *
 * and
 *
 *    exec( cmdLine ) - executes the specified command line, then
 *                      restarts execution of the current script.
 *                      Note that this differs from popen in that
 *                      the current Javascript interpreter is
 *                      shut down before execution. This allows
 *                      the exec'd program access to the touch
 *                      screen and audio devices. (Useful for 
 *                      calibrating touch screen).
 *
 *
 * Change History : 
 *
 * $Log: jsHyperlink.h,v $
 * Revision 1.4  2008-12-02 00:22:47  ericn
 * use exec, not system to launch another program
 *
 * Revision 1.3  2003-02-27 03:50:58  ericn
 * -added exec routine
 *
 * Revision 1.2  2002/12/01 02:40:36  ericn
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
#include <vector>

extern bool volatile gotoCalled_ ;
extern std::string   gotoURL_ ;
extern bool volatile execCalled_ ;
extern std::string   execCmd_ ;
extern std::vector<std::string> execCmdArgs_ ;

bool initJSHyperlink( JSContext *cx, JSObject *glob );

#endif

