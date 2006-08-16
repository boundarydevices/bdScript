/*
 * Module rtSignal.cpp
 *
 * This module defines the trival routine nextRtSignal()
 * for use in allocating signal numbers to an application.
 *
 * Change History : 
 *
 * $Log: rtSignal.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "rtSignal.h"
#include <signal.h>

static int rts_ = SIGRTMIN ;

int nextRtSignal( void ){
   return rts_++ ;
}
