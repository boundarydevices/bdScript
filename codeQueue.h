#ifndef __CODEQUEUE_H__
#define __CODEQUEUE_H__ "$Id: codeQueue.h,v 1.9 2003-07-06 01:21:37 ericn Exp $"

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
 * Revision 1.9  2003-07-06 01:21:37  ericn
 * -added method abortCodeQueue()
 *
 * Revision 1.8  2003/06/08 15:19:59  ericn
 * -added support for queued function call
 *
 * Revision 1.7  2003/01/06 04:29:49  ericn
 * -modified to allow unlink() of filter before destruction
 *
 * Revision 1.6  2002/12/01 15:56:48  ericn
 * -added filter support
 *
 * Revision 1.5  2002/12/01 03:14:21  ericn
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
#include "dlList.h"

//
// returns true if queued successfully, 
// false if the program is aborting.
//
// Use this routine only for rooted code
//
bool queueSource( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile,
                  unsigned    argc = 0,
                  jsval      *argv = 0 );
//
// same as queueSource for unrooted code.
//
bool queueUnrootedSource( JSObject   *scope,
                          jsval       sourceCode,
                          char const *sourceFile,
                          unsigned    argc = 0,
                          jsval      *argv = 0 );

typedef void (*callback_t)( void *cbData );

bool queueCallback( callback_t callback,
                    void      *cbData );

//
// use this to simplify callbacks
//
void executeCode( JSObject   *scope,
                  jsval       sourceCode,
                  char const *sourceFile,
                  unsigned    argc = 0,
                  jsval      *argv = 0 );


class codeFilter_t {
public:
   codeFilter_t( void );
   virtual ~codeFilter_t( void ); // removes itself from the chain

   //
   // the default returns false.
   // override this to divert callbacks.
   //
   // return true if your class handled the callback,
   // false to let it pass into the queue.
   //
   // if you return true, the wait() call will return so
   // that your code can check status
   //
   virtual bool isHandled( callback_t callback,
                           void      *cbData );

   //
   // Call this to wait for events. Any messages in the
   // queue will be ignored.
   //
   void wait( void );

   //
   // Override this to indicate completion. When this
   // returns true, the wait() routine will return.
   //
   virtual bool isDone( void );

   void unlink( void );
private:
   codeFilter_t( codeFilter_t const & ); // no copies

   friend bool queueCallback( callback_t callback,
                              void      *cbData );

   list_head   chain_ ;
};


//
// returns when idle for specified time in milliseconds,
// when 'iterations' fragments of code have been executed,
// or upon a 'gotoURL' call
//
void pollCodeQueue( JSContext *cx,
                    unsigned   milliseconds,
                    unsigned   iterations );

//
// called to terminate the normal read cycle of the code 
// queue in response to a gotoURL(), exit(), or exec() call.
//
void abortCodeQueue( void );

//
// This should be called after the initial script is 
// evaluated to establish a run-time context
//
void initializeCodeQueue( JSContext *cx,
                          JSObject  *glob );

#endif

