#ifndef __TICKMS_H__
#define __TICKMS_H__ "$Id: tickMs.h,v 1.3 2003-10-05 19:11:37 ericn Exp $"

/*
 * tickMs.h
 *
 * This header file declares the tickMs() routine, 
 * which returns a tick counter in milliseconds.
 *
 *
 * Change History : 
 *
 * $Log: tickMs.h,v $
 * Revision 1.3  2003-10-05 19:11:37  ericn
 * -added timeValToMs() call
 *
 * Revision 1.2  2003/08/11 00:10:43  ericn
 * -prevent #include dependency
 *
 * Revision 1.1  2003/07/27 15:19:12  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include <sys/time.h>

inline long long timeValToMs( struct timeval const &tv )
{
   return ((long long)tv.tv_sec*1000)+((long long)tv.tv_usec / 1000 );
}

inline long long tickMs()
{
   struct timeval now ; gettimeofday( &now, 0 );
   return timeValToMs( now );
}

#endif

