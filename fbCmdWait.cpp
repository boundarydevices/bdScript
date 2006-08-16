/*
 * Module fbCmdWait.cpp
 *
 * This module defines the methods of the fbWait_t 
 * class as declared in fbCmdWait.h
 *
 *
 * Change History : 
 *
 * $Log: fbCmdWait.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbCmdWait.h"

#include <linux/sm501-int.h>

// Note that the documentation has missing low-order bits!

static unsigned long const bitmask = ( 0x3ff << 11 ) | 7 ;

fbWait_t::fbWait_t( unsigned long bitsOfInterest,
                        unsigned long values )
   : fbCommand_t(sizeof(data_))
{
   data_[0] = 0x60000000 | ( values & bitmask );
   data_[1] = bitsOfInterest & bitmask ;
}

fbWait_t::~fbWait_t( void )
{
}

void const *fbWait_t::data( void ) const 
{
   return data_ ;
}

