#ifndef __JSFILEIO_H__
#define __JSFILEIO_H__ "$Id: jsFileIO.h,v 1.1 2003-03-04 14:45:18 ericn Exp $"

/*
 * jsFileIO.h
 *
 * This header file declares the initialization routine
 * for the FILE I/O Javascript routines:
 *
 *    readFile( filename );            - returns string with file content
 *    writeFile( filename, data );     - writes data to filename, returns true if successful
 *    unlink( filename );              - deletes filename, returns true if it works
 *    copyFile( srcfile, destfile );   - copies srcfile to destfile, returns true if copied
 *    renameFile( srcfile, destfile ); - renames (mv's) srcfile to destfile, returns true if moved
 *
 * Change History : 
 *
 * $Log: jsFileIO.h,v $
 * Revision 1.1  2003-03-04 14:45:18  ericn
 * -added jsFileIO module
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSFileIO( JSContext *cx, JSObject *glob );

#endif

