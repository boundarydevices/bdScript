#ifndef __CODEQUEUE_H__
#define __CODEQUEUE_H__ "$Id: codeQueue.h,v 1.1 2002-10-27 17:42:08 ericn Exp $"

/*
 * codeQueue.h
 *
 * This header file declares the interface to the
 * javascript code queue.
 *
 * There are two primary operations:
 *    queueSource
 *    dequeue
 *
 * as well as an initialization routine to establish
 * the global context for the interpreter.
 * 
 *
 * Change History : 
 *
 * $Log: codeQueue.h,v $
 * Revision 1.1  2002-10-27 17:42:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <string>
#include "js/jsapi.h"

//
// returns true if compiled and queued successfully, 
// false if the code couldn't be compiled 
//
bool queueSource( std::string const &sourceCode,
                  char const        *sourceFile );

//
// returns a valid pointer if successful. 
//
JSScript *dequeueByteCode( unsigned long milliseconds = 0xFFFFFFFF );

//
// This should be called after the initial script is 
// evaluated to establish a run-time context
//
void initializeCodeQueue( JSContext *cx,
                          JSObject  *glob );

#endif

