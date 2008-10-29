/*
 * Module fbCmdListSignal.cpp
 *
 * This module defines the methods of the fbCmdListSignal_t
 * class as declared in fbCmdListSignal.h
 *
 *
 * Change History : 
 *
 * $Log: fbCmdListSignal.cpp,v $
 * Revision 1.4  2008-10-29 15:27:46  ericn
 * -prevent compiler warnings
 *
 * Revision 1.3  2007-08-23 00:25:54  ericn
 * -add CLOEXEC for file handles
 *
 * Revision 1.2  2007/08/08 17:08:18  ericn
 * -[sm501] Use class names from sm501-int.h
 *
 * Revision 1.1  2006/10/16 22:45:33  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbCmdListSignal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "rtSignal.h"
#include <unistd.h>
#include <linux/sm501-int.h>
#include "fbDev.h"
#include <fcntl.h>
#include <sys/ioctl.h>

static fbCmdListSignal_t *instance_ = 0 ;

fbCmdListSignal_t &fbCmdListSignal_t::get()
{
   if( 0 == instance_ )
      instance_ = new fbCmdListSignal_t();
   return *instance_ ;
}

fbCmdListSignal_t::fbCmdListSignal_t( void )
   : fd_( open( "/dev/" SM501CMD_CLASS, O_RDWR ) )
   , signal_( nextRtSignal() )
{
   if( isOpen() ){
      fcntl( fd_, F_SETFD, FD_CLOEXEC);
      fcntl( fd_, F_SETOWN, getpid());
      fcntl( fd_, F_SETSIG, signal_);
      
      fbDevice_t &fb = getFB();
      reg_and_value rv ;
      rv.reg_   = SMICMD_CONDITION ;
      rv.value_ = 1 ; // condition bit for skip commands

      int res = ioctl( fb.getFd(), SM501_WRITEREG, &rv );
      if( res )
         perror( "SM501_WRITEREG" );
   }
}

fbCmdListSignal_t::~fbCmdListSignal_t( void )
{
   if( isOpen() ){
      close( fd_ );
   }
}

void fbCmdListSignal_t::enable()
{
   if( isOpen() ){
      int flags = fcntl( fd_, F_GETFL, 0 );
      fcntl( fd_, F_SETFL, flags | O_NONBLOCK | FASYNC );
   }
}

void fbCmdListSignal_t::disable()
{
   if( isOpen() ){
      int flags = fcntl( fd_, F_GETFL, 0 );
      fcntl( fd_, F_SETFL, flags & ~FASYNC );
   }
}

