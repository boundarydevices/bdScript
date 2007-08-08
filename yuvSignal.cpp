/*
 * Module yuvSignal.cpp
 *
 * This module defines the methods of the yuvSignal_t
 * class as declared in yuvSignal.h
 *
 * Change History : 
 *
 * $Log: yuvSignal.cpp,v $
 * Revision 1.2  2007-08-08 17:08:29  ericn
 * -[sm501] Use class names from sm501-int.h
 *
 * Revision 1.1  2006/10/16 22:45:55  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "yuvSignal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "rtSignal.h"
#include <unistd.h>

static yuvSignal_t *instance_ = 0 ;

yuvSignal_t &yuvSignal_t::get()
{
   if( 0 == instance_ )
      instance_ = new yuvSignal_t();
   return *instance_ ;
}

yuvSignal_t::yuvSignal_t( void )
   : fd_( open( "/dev/" SM501YUV_CLASS, O_RDWR ) )
   , signal_( nextRtSignal() )
{
   if( isOpen() ){
      fcntl( fd_, F_SETOWN, getpid());
      fcntl( fd_, F_SETSIG, signal_);
   }
}

yuvSignal_t::~yuvSignal_t( void )
{
   if( isOpen() ){
      disable();
      close( fd_ );
   }
}

void yuvSignal_t::enable()
{
   if( isOpen() ){
      int flags = flags = fcntl( fd_, F_GETFL, 0 );
      fcntl( fd_, F_SETFL, flags | O_NONBLOCK | FASYNC );
   }
}

void yuvSignal_t::disable()
{
   if( isOpen() ){
      int flags = flags = fcntl( fd_, F_GETFL, 0 );
      fcntl( fd_, F_SETFL, flags & ~FASYNC );
   }
}

