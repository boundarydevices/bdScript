#ifndef __DEBUGPRINT_H__
#define __DEBUGPRINT_H__ "$Id: debugPrint.h,v 1.2 2004-07-04 21:33:16 ericn Exp $"

/*
 * debugPrint.h
 *
 * This header file declares the debugPrint() routine,
 * which is used to print debug information if the DEBUGPRINT
 * macro is set.
 *
 *
 * Change History : 
 *
 * $Log: debugPrint.h,v $
 * Revision 1.2  2004-07-04 21:33:16  ericn
 * -added debugHex() routine
 *
 * Revision 1.1  2003/11/24 19:42:42  ericn
 * -polling touch screen
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#ifdef DEBUGPRINT
#include <stdio.h>
#include <stdarg.h>
#include "hexDump.h"

inline int debugPrint( char const *fmt, ... )
{
   va_list ap;
   va_start( ap, fmt );
   return vfprintf( stdout, fmt, ap );
}

inline void debugHex( char const *label, void const *data, unsigned size )
{
   dumpHex( label, data, size );
}

#else

inline int debugPrint( char const *, ... )
{
}

inline void debugHex( char const *, void const *, unsigned )
{
}

#endif

#endif

