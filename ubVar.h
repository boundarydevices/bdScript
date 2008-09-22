#ifndef __UBVAR_H__
#define __UBVAR_H__ 

/*
 * ubVar.h
 *
 * This header file declares the readFlashVar() and writeFlashVar()
 * routines, which will get or set U-Boot environment variables in 
 * /dev/mtd0.
 *
 * Copyright Boundary Devices, Inc. 2008
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
void writeFlashVar( char const *name, char const *value );

#endif

