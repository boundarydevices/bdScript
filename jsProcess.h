#ifndef __JSPROCESS_H__
#define __JSPROCESS_H__ "$Id: jsProcess.h,v 1.1 2003-08-24 17:32:17 ericn Exp $"

/*
 * jsProcess.h
 *
 * This header file declares the initialization routine
 * for the Javascript process-control wrapper(s).
 *
 * Current implementation only has a single class:
 *
 *    childProcess( programName, exitHandler [,arg strings] ) 
 *
 *       This class defines a child process which 
 *          inherits no file descriptors or pipes
 *
 *          receives notification of child termination
 *             via the exitHandler parameter. This parameter
 *             should be a function which runs in the 
 *             context of the childProcess object.
 *
 *          defines a "close" method to kill the child
 *
 *          defines an "isRunning()" method to tell if
 *             the child is alive
 *
 *          defines a "pid" member corresponding to
 *             the child's process id
 *
 * Note that all child processes are killed at process exit.
 *
 * Change History : 
 *
 * $Log: jsProcess.h,v $
 * Revision 1.1  2003-08-24 17:32:17  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSProcess( JSContext *cx, JSObject *glob );

#endif

