/*
 * Module blockSig.cpp
 *
 * This module defines the methods of the blockSignal_t
 * class as declared in blockSig.h
 *
 *
 * Change History : 
 *
 * $Log: blockSig.cpp,v $
 * Revision 1.1  2006-10-16 22:45:27  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "blockSig.h"
#include <signal.h>
#include <assert.h>

blockSignal_t::blockSignal_t( int signo )
   : sig_( signo )
   , wasMasked_( 0 )
{
   sigset_t blockThese ;
   sigset_t oldSet ;
   sigemptyset( &blockThese );
   sigaddset( &blockThese, signo );

   int rc = pthread_sigmask( SIG_BLOCK, &blockThese, &oldSet );
   assert( 0 == rc );

   wasMasked_ = sigismember( &oldSet, signo );
}

blockSignal_t::~blockSignal_t( void )
{
   if( !wasMasked_ ){
      sigset_t unblock ;
      sigemptyset( &unblock );
      sigaddset( &unblock, sig_ );

      int rc = pthread_sigmask( SIG_UNBLOCK, &unblock, 0 );
      assert( 0 == rc );
   } // only undo our own operation
}

