#ifndef __JSFILEIO_H__
#define __JSFILEIO_H__ "$Id: jsFileIO.h,v 1.4 2004-04-20 15:18:08 ericn Exp $"

/*
 * jsFileIO.h
 *
 * This header file declares the initialization routine
 * for the FILE I/O Javascript routines:
 *
 *    readFile( filename );            - returns string with file content
 *    writeFile( filename,             - writes data to filename, returns true if successful
 *               data,
 *               [timestamp] );        - optional timestamp
 *    unlink( filename );              - deletes filename, returns true if it works
 *    copyFile( srcfile, destfile );   - copies srcfile to destfile, returns true if copied
 *    renameFile( srcfile, destfile ); - renames (mv's) srcfile to destfile, returns true if moved
 *    stat( filename )                 - returns an object with the details of a struct stat (Posix)
 *                                       (members don't have the st_ prefixes)
 *    touch( filename, [timestamp] );  - changes modification date 
 * 
 * and the 'file' class, which is used to represent an open file handle
 * for continuous reads and writes.
 *
 * The 'file' class contains the following members:
 *
 *    constructor file( "path", "modestring" );    - modestring containing 'w' and 'r', always text mode
 * 
 *    isOpen()   - returns true if file handle is open
 *    path       - readonly data member (string)
 *    mode       - readonly data member (string)
 *    close()    - close and invalidate file handle
 *
 *    --- if read permission
 *
 *    onCharIn( function, obj )  - set charIn callback, signature obj.function( file )
 *    onLineIn( function, obj )  - set line in callback, signature obj.function( file )
 *    string = read()            ( non-blocking, returns false if nothing available )
 *
 *    --- if write permission
 *    bool write( string )
 *
 * Change History : 
 *
 * $Log: jsFileIO.h,v $
 * Revision 1.4  2004-04-20 15:18:08  ericn
 * -Added file class (for devices)
 *
 * Revision 1.3  2003/08/31 16:52:43  ericn
 * -added optional timestamp to writeFile, added touch() routine
 *
 * Revision 1.2  2003/08/31 15:06:29  ericn
 * -added method stat
 *
 * Revision 1.1  2003/03/04 14:45:18  ericn
 * -added jsFileIO module
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "js/jsapi.h"

bool initJSFileIO( JSContext *cx, JSObject *glob );

#endif

