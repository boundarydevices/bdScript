#ifndef __DEBUGPRINT_H__
#define __DEBUGPRINT_H__ "$Id: debugPrint.h,v 1.1 2003-11-24 19:42:42 ericn Exp $"

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
 * Revision 1.1  2003-11-24 19:42:42  ericn
 * -polling touch screen
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#ifdef DEBUGPRINT
#include <stdio.h>
#include <stdarg.h>

inline int debugPrint( char const *fmt, ... )
{
   va_list ap;
   va_start( ap, fmt );
   return vprintf( fmt, ap );
}

#else

inline int debugPrint( char const *fmt, ... )
{
}

#endif

#endif

