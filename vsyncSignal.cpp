/*
 * Module vsyncSignal.cpp
 *
 * This module defines the methods of the vsyncSignal_t class
 * as declared in vsyncSignal.h
 *
 *
 * Change History : 
 *
 * $Log: vsyncSignal.cpp,v $
 * Revision 1.3  2007-08-23 00:25:52  ericn
 * -add CLOEXEC for file handles
 *
 * Revision 1.2  2007/08/08 17:08:28  ericn
 * -[sm501] Use class names from sm501-int.h
 *
 * Revision 1.1  2006/10/16 22:45:53  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "vsyncSignal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "rtSignal.h"
#include <unistd.h>
#include <linux/sm501-int.h>

static vsyncSignal_t *instance_ = 0 ;

vsyncSignal_t &vsyncSignal_t::get()
{
   if( 0 == instance_ )
      instance_ = new vsyncSignal_t();
   return *instance_ ;
}

vsyncSignal_t::vsyncSignal_t( void )
   : fd_( open( "/dev/" SM501INT_CLASS, O_RDONLY ) )
   , signal_( nextRtSignal() )
{
   if( isOpen() ){
      fcntl( fd_, F_SETFD, FD_CLOEXEC);
      fcntl( fd_, F_SETOWN, getpid());
      fcntl( fd_, F_SETSIG, signal_);
   }
}

vsyncSignal_t::~vsyncSignal_t( void )
{
   if( isOpen() ){
      close( fd_ );
   }
}

void vsyncSignal_t::enable()
{
   if( isOpen() ){
      int flags = flags = fcntl( fd_, F_GETFL, 0 );
      fcntl( fd_, F_SETFL, flags | O_NONBLOCK | FASYNC );
   }
}

void vsyncSignal_t::disable()
{
   if( isOpen() ){
      int flags = flags = fcntl( fd_, F_GETFL, 0 );
      fcntl( fd_, F_SETFL, flags & ~FASYNC );
   }
}

