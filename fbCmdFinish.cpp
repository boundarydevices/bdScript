/*
 * Module fbCmdFinish.cpp
 *
 * This module defines the methods of the fbFinish_t
 * class as declared in fbCmdFinish.h
 *
 *
 * Change History : 
 *
 * $Log: fbCmdFinish.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "fbCmdFinish.h"

#include <linux/sm501-int.h>

static unsigned long const finishCmd_[] = {
    0x80000001    // FINISH
,   0x00000000    // PAD
};

fbFinish_t::fbFinish_t()
   : fbCommand_t(sizeof(finishCmd_))
{
}

fbFinish_t::~fbFinish_t( void )
{
}

void const *fbFinish_t::data( void ) const 
{
   return finishCmd_ ;
}

