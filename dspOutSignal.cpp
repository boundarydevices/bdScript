/*
 * Module dspOutSignal.cpp
 *
 * This module defines the methods of the dspOutSignal_t 
 * class as declared in dspOutSignal.h
 *
 *
 * Change History : 
 *
 * $Log: dspOutSignal.cpp,v $
 * Revision 1.1  2006-10-16 22:45:29  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "dspOutSignal.h"
#include "multiSignal.h"
#include "rtSignal.h"
#include <linux/soundcard.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

static int dspOutSignal_ = 0 ;

dspOutSignal_t::dspOutSignal_t( void )
   : fd_( open( "/dev/dsp", O_WRONLY ) )
   , sig_( 0 )
{
   // re-use the same signal
   if( 0 == dspOutSignal_ )
      dspOutSignal_ = nextRtSignal();

   sig_ = dspOutSignal_ ;
   if( isOpen() ){
      int const format = AFMT_S16_LE ;
      if( 0 != ioctl( fd_, SNDCTL_DSP_SETFMT, &format) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
      
      int vol = (75<<8)|75 ;
      if( 0 > ioctl( fd_, SOUND_MIXER_WRITE_VOLUME, &vol)) 
         perror( "Error setting volume" );
      fcntl( fd_, F_SETOWN, getpid());
      fcntl( fd_, F_SETSIG, sig_);
   }
}

dspOutSignal_t::~dspOutSignal_t( void )
{
   if( isOpen() ){
      close( fd_ );
      fd_ = -1 ;
   }
}

void dspOutSignal_t::enable()
{
   if( isOpen() ){
      int flags = flags = fcntl( fd_, F_GETFL, 0 );
      fcntl( fd_, F_SETFL, flags | O_NONBLOCK | FASYNC );
   }
}

void dspOutSignal_t::disable()
{
   if( isOpen() ){
      int flags = flags = fcntl( fd_, F_GETFL, 0 );
      fcntl( fd_, F_SETFL, flags & ~FASYNC );
   }
}

