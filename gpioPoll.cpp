/*
 * Module gpioPoll.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: gpioPoll.cpp,v $
 * Revision 1.1  2003-10-05 19:15:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "gpioPoll.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

gpioPoll_t :: gpioPoll_t
   ( pollHandlerSet_t &set,
     char const       *devName )
   : pollHandler_t( open( devName, O_RDWR ), set )
   , devName_( strdup( devName ) )
   , isHigh_( 0 )
{
   if( isOpen() )
   {
      fcntl( fd_, F_SETFD, FD_CLOEXEC );
      fcntl( fd_, F_SETFL, O_NONBLOCK );
      setMask( POLLIN );
      set.add( *this );
   }
}

gpioPoll_t :: ~gpioPoll_t( void )
{
   if( isOpen() )
   {
      close( fd_ );
      fd_ = -1 ;
   }
   free( devName_ );
}

void gpioPoll_t :: onHigh( void )
{
   printf( "%s high\n", devName_ );
}

void gpioPoll_t :: onLow( void )
{
   printf( "%s low\n", devName_ );
}

void gpioPoll_t :: onTransition( void )
{
   if( isHigh_ )
      onHigh();
   else
      onLow();
}

void gpioPoll_t :: onDataAvail( void )
{
   char ch;
   int numRead ;
   if( (numRead = read( fd_, &ch, 1)) >= 0)
   {
      isHigh_ = ( 0 != (ch&1) );
      onTransition();
   }
}

