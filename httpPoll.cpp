/*
 * Module httpPoll.cpp
 *
 * This module defines the methods of the httpPoll_t class
 * as declared in httpPoll.h
 *
 *
 * Change History : 
 *
 * $Log: httpPoll.cpp,v $
 * Revision 1.1  2004-03-17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "httpPoll.h"
#include <stdio.h>

httpPoll_t :: httpPoll_t
   ( char const *path )
   : state_( idle )
{
}

httpPoll_t :: ~httpPoll_t( void )
{
}

void httpPoll_t :: addHeader( std::string const &fieldName, std::string const &value )
{
   header_t h ;
   h.fieldName_ = fieldName ;
   h.value_     = value ;
   headers_.push_back( h );
}

void httpPoll_t :: addParam( std::string const &fieldName, std::string const &value )
{
   header_t h ;
   h.fieldName_ = fieldName ;
   h.value_     = value ;
   params_.push_back( h );
}

void httpPoll_t :: addFile( std::string const &fieldName, std::string const &fileName )
{
   header_t h ;
   h.fieldName_ = fieldName ;
   h.value_     = fileName ;
   files_.push_back( h );
}

void httpPoll_t :: start( void )
{
   if( idle == getState() )
   {
   }
   else
      fprintf( stderr, "Duplicate or invalid httpPoll_t::start(): %d\n", getState() );
}

bool httpPoll_t :: getAuth( char const *&username, char const *&password )
{
   username = 0 ;
   password = 0 ;
   return false ;
}

void httpPoll_t :: onWriteData( unsigned long writtenSoFar )
{
}

void httpPoll_t :: writeSize( unsigned long numToWrite )
{
}

void httpPoll_t :: readSize( unsigned long numBytes )
{
}

void httpPoll_t :: onReadData( unsigned long readSoFar )
{
}

void httpPoll_t :: onConnect( tcpHandler_t &h )
{
}

void httpPoll_t :: onDisconnect( tcpHandler_t &h )
{
   h.removeClient( *this );
   if( complete != getState() )
      state_ = error ;
}

void httpPoll_t :: onDataAvail( pollHandler_t &h )
{
}

void httpPoll_t :: onWriteSpace( pollHandler_t &h )
{
}

void httpPoll_t :: onError( pollHandler_t &h )
{
   onDisconnect( *(tcpHandler_t *)&h );
}

void httpPoll_t :: onHUP( pollHandler_t &h )
{
   onDisconnect( *(tcpHandler_t *)&h );
}
