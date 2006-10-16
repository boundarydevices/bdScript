#ifndef __SIGNALFLASH_H__
#define __SIGNALFLASH_H__ "$Id: signalFlash.h,v 1.1 2006-10-16 22:45:49 ericn Exp $"

/*
 * signalFlash.h
 *
 * This header file declares the signalFlash_t class, which 
 * is used to play a flash animation in a signal-driven 
 * application.
 *
 *
 * Change History : 
 *
 * $Log: signalFlash.h,v $
 * Revision 1.1  2006-10-16 22:45:49  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "parsedFlash.h"

class signalFlash_t {
public:
   parsedFlash_t parsed_ ;
};

#endif

