#ifndef __JSGLOBALS_H__
#define __JSGLOBALS_H__ "$Id: jsGlobals.h,v 1.5 2004-09-25 14:16:54 ericn Exp $"

/*
 * jsGlobals.h
 *
 * This header file declares the execution globals
 * for the testEvents and testJS programs:
 *
 *    execMutex_ ;      - used to ensure that only one
 *                        thread is manipulating the Javascript context
 *    execContext_ ;    - used to provide context of main Javascript file
 *
 * Change History : 
 *
 * $Log: jsGlobals.h,v $
 * Revision 1.5  2004-09-25 14:16:54  ericn
 * -made global_class global
 *
 * Revision 1.4  2004/07/04 21:32:07  ericn
 * -added alignment constants (generally useful)
 *
 * Revision 1.3  2003/11/22 21:02:37  ericn
 * -made code queue a pollHandler_t
 *
 * Revision 1.2  2002/11/30 00:53:09  ericn
 * -changed name to semClasses.h
 *
 * Revision 1.1  2002/10/31 02:13:08  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#ifndef __SEMAPHORE_H__
#include "semClasses.h"
#endif

#ifndef __POLLHANDLER_H__
#include "pollHandler.h"
#endif

#include "js/jsapi.h"

extern mutex_t           execMutex_ ;
extern JSContext        *execContext_ ;
extern pollHandlerSet_t  pollHandlers_ ;
extern JSClass           global_class ;

enum {
   alignCenterHorizontal   = 1,
   alignRight              = 2,
   alignBottom             = 4,
   alignCenterVertical     = 8
};

#endif

