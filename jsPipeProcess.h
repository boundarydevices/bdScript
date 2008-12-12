#ifndef __JSPIPEPROCESS_H__
#define __JSPIPEPROCESS_H__ "$Id: jsPipeProcess.h,v 1.1 2008-12-12 01:24:47 ericn Exp $"

/*
 * jsPipeProcess.h
 *
 * This header file declares the entry points for the pipeProcess
 * Javascript class, which can be used to start up a child process
 * with its' stdin, stdout, and stderr re-directed to pipes.
 *
 * A pipeProcess object is constructed using an anonymous object
 * initializer with a set of the following members:
 *
 *    progName:   [required] fully-qualified path to the executable
 *    args:       [optional] set of string command-line arguments
 *    onStdin:    [optional] handler for stdin readiness (meaning that
 *                   space is available for writing). This routine
 *                   will only be called once until a subsequent call to
 *                   write() fails for insufficient space (i.e. return 0).
 *    onStdout:   [optional] handler for stdout readiness (meaning that
 *                   one or more bytes of input is available). The routine
 *                   must call the read(stdout) method until no more data is 
 *                   returned or the onStdout routine will be called again 
 *                   during the next iteration of the main loop.
 *    onStderr:   [optional] handler for stderr readiness. The routine must
 *                   call the read(stderr) routine until no more data is
 *                   available.
 *    onClose:    [optional] handler for a POLLHUP event on any of the stdin, stdout, 
 *                   or stderr file descriptors. The routine is called with a 
 *                   parameter of 0, 1, or 2 corresponding to stdin, stdout, 
 *                   and stderr respectively.
 *
 *                   function onClose(fdNum){
 *                      print( "child file handle ", fdNum, " closed\n" );
 *                   }
 *    onError:    [optional] handler for a POLLERR event on any of the stdin, stdout,
 *                   or stderr file descriptors. Same signature as the onClose()
 *                   method.
 *    onExit:     [optional] called once when the child process dies.
 *
 * Note that the onStdin, onStdout, onStderr, and onClose routines will be 
 * called in the context of the pipeProcess object, so they'll have access to
 * any static data.
 * 
 * Also note that these routines are stored in members of the same name within
 * the pipeProcess object so they can be replaced at run-time.
 *
 * The pipeProcess class contains the following member variables:
 *    stdin    - (integer) 0 - readonly
 *    stdout   - (integer) 1 - readonly
 *    stderr   - (integer) 2 - readonly
 *
 *    pid      - (integer) process id of child
 *
 * The pipeProcess class contains the following methods:
 *
 *    close(fdNum) - closes one of stdin, stdout, stderr
 *    kill()       - shuts down the child process by first closing each file handle, 
 *                   then issuing a kill() on the process, then waiting for completion.
 *    isRunning()  - tests the child process to see if it's alive (through kill(0)).
 *    read(fdNum)  - returns a string with content of data from the specified file 
 *                   handle (either stdout or stderr) or false if nothing is available.
 *    write(str)   - writes the specified string to the child process's stdin file handle.
 *                   Returns the number of bytes written or 'false' if an error occurs.
 *
 * Change History : 
 *
 * $Log: jsPipeProcess.h,v $
 * Revision 1.1  2008-12-12 01:24:47  ericn
 * added pipeProcess class
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "js/jsapi.h"

bool initJSPipeProcess( JSContext *cx, JSObject *glob );

#endif

