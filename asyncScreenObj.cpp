/*
 * Module asyncScreenObj.cpp
 *
 * This module defines the methods of the asyncScreenObject_t
 * class as declared in asyncScreenObj.h
 *
 * Change History : 
 *
 * $Log: asyncScreenObj.cpp,v $
 * Revision 1.1  2006-10-16 22:45:26  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "asyncScreenObj.h"
#include <assert.h>

asyncScreenObject_t::asyncScreenObject_t(fbCommandList_t &cmdList)
   : cmdList_( cmdList )
   , setValueFlag_( 0 )
{
}

asyncScreenObject_t::~asyncScreenObject_t()
{
}

void asyncScreenObject_t::executed()
{
}

bool asyncScreenObject_t::valueBeingSet(void) const {
   return (0 != setValueFlag_);
}

void asyncScreenObject_t::setValueStart(){
   assert( 0 == setValueFlag_ );
   setValueFlag_ = 1 ;
}
   
void asyncScreenObject_t::setValueEnd()
{
   assert( 0 != setValueFlag_ );
   setValueFlag_ = 0 ;
}

