/*
 * Module gpioPoll.cpp
 *
 * This module defines the methods of the gpioPoll_t 
 * class as declared in gpioPoll.h
 *
 *
 * Change History : 
 *
 * $Log: gpioPoll.cpp,v $
 * Revision 1.3  2004-01-01 20:11:42  ericn
 * -added isOpen() routine, and switched pollHandlers to use close()
 *
 * Revision 1.2  2003/10/18 19:15:06  ericn
 * -added comment
 *
 * Revision 1.1  2003/10/05 19:15:44  ericn
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
      close();
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

