#ifndef __CODEQUEUE_H__
#define __CODEQUEUE_H__ "$Id: codeQueue.h,v 1.5 2002-12-01 03:14:21 ericn Exp $"

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
 * Revision 1.5  2002-12-01 03:14:21  ericn
 * -added executeCode() method for handlers
 *
 * Revision 1.4  2002/12/01 02:42:04  ericn
 * -added queueCallback() and queueUnrootedSource(), changed dequeueByteCode() to pollCodeQueue()
 *
 * Revision 1.3  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.2  2002/10/31 02:09:35  ericn
 * -added scope to code queue
 *
 * Revision 1.1  2002/10/27 17:42:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <string>
#include "js/jsapi.h"

//
// returns true if queued successfully, 
// false if the program is aborting.
//
// Use this routine only for rooted code
//
bool queueSource( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile );
//
// same as queueSource for unrooted code.
//
bool queueUnrootedSource( JSObject   *scope,
                          jsval       sourceCode,
                          char const *sourceFile );

typedef void (*callback_t)( void *cbData );

bool queueCallback( callback_t callback,
                    void      *cbData );

//
// use this to simplify callbacks
//
void executeCode( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile );

//
// returns when idle for specified time in milliseconds,
// when 'iterations' fragments of code have been executed,
// or upon a 'gotoURL' call
//
void pollCodeQueue( JSContext *cx,
                    unsigned   milliseconds,
                    unsigned   iterations );

//
// This should be called after the initial script is 
// evaluated to establish a run-time context
//
void initializeCodeQueue( JSContext *cx,
                          JSObject  *glob );

#endif

