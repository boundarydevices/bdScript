/*
 * Module jsGlobals.cpp
 *
 * This module defines the Javascript globals
 * as declared in jsGlobals.h
 *
 *
 * Change History : 
 *
 * $Log: jsGlobals.cpp,v $
 * Revision 1.2  2002-12-02 15:08:55  ericn
 * -added mutex name
 *
 * Revision 1.1  2002/10/31 02:13:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsGlobals.h"

mutex_t execMutex_( "jsExecMutex" );
JSContext *execContext_ = 0 ;


