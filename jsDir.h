#ifndef __JSDIR_H__
#define __JSDIR_H__ "$Id: jsDir.h,v 1.1 2003-08-31 19:31:54 ericn Exp $"

/*
 * jsDir.h
 *
 * This header file declares the initialization routine
 * for a set of (one) Javascript directory-access functions.
 *
 *    readDir( directory )    - returns an object representing
 *                              the directory tree.
 *
 * Notes:
 *
 *    The readDir() routine does not follow symlinks.
 *
 *    Returns a recursive anonymous object with the following
 *    structure:
 *
 *       {
 *          'fileNode': {
 *             time: timestamp,  // Seconds since the epoch
 *             size: inbytes,
 *             mode: nnnn
 *          },
 *          'dirNode': {
 *             'fileNode2': {
 *                time: timestamp,  // Seconds since the epoch
 *                size: inbytes,
 *                mode: nnnn
 *             },
 *             'fileNode3': {
 *                time: timestamp,  // Seconds since the epoch
 *                size: inbytes,
 *                mode: nnnn
 *             }
 *             ...
 *          }
 *       }
 * 
 *    The return value is essentially a set of associative arrays
 *    where each node represents either a directory or file (use
 *    the test 'null != node.size' to determine which). 
 *    Directory nodes have the same structure as the parent object.
 *
 * Change History : 
 *
 * $Log: jsDir.h,v $
 * Revision 1.1  2003-08-31 19:31:54  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSDir( JSContext *cx, JSObject *glob );

#endif

