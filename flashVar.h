#ifndef __FLASHVAR_H__
#define __FLASHVAR_H__ "$Id: flashVar.h,v 1.2 2008-09-21 21:55:08 ericn Exp $"

/*
 * flashVar.h
 *
 * This header file declares the readFlashVar() and writeFlashVar()
 * routines, which will get or set flash variables in the last
 * sector of /dev/mtd2
 *
 *
 * Change History : 
 *
 * $Log: flashVar.h,v $
 * Revision 1.2  2008-09-21 21:55:08  ericn
 * [flashVar] return success/fail status from writeFlashVar
 *
 * Revision 1.1  2004-02-01 09:53:18  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


//
// returns variable if found, NULL otherwise
//
// free() when done with result
//
char const *readFlashVar( char const *varName );

//
// writes the specified flash variable with the specified value
//
bool writeFlashVar( char const *name,
                    char const *value );

#endif

