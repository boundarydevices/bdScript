/*
 * Module parsedFlash.cpp
 *
 * This module defines the methods of the parsedFlash_t
 * class as declared in parsedFlash.h
 *
 *
 * Change History : 
 *
 * $Log: parsedFlash.cpp,v $
 * Revision 1.1  2003-11-24 19:42:42  ericn
 * -polling touch screen
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "parsedFlash.h"
#include <string.h>

parsedFlash_t :: parsedFlash_t( void const *data, unsigned dataBytes )
   : hFlash_( FlashNew() )
   , parsed_( false )
{
   int status ;

   // Load level 0 movie
   do {
      status = FlashParse( hFlash_, 0, (char *)data, dataBytes );
   } while( status & FLASH_PARSE_NEED_DATA );
   
   parsed_ = ( FLASH_PARSE_START & status );
   if( parsed_ )
      FlashGetInfo( hFlash_, &fi_ );
   else
      memset( &fi_, 0, sizeof( fi_ ) );
}

parsedFlash_t :: ~parsedFlash_t( void )
{
   FlashClose( hFlash_ );
}

