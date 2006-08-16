#ifndef __ODOMCOMMAND_H__
#define __ODOMCOMMAND_H__ "$Id: odomCommand.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * odomCommand.h
 *
 * This header file declares the odomCmdInterp_t class 
 * for use in controlling a set of odometers from varying
 * inputs (tty, serial, UDP).
 *
 * The only state that the odomCommand_t has is 
 * an error message buffer and a 'die' or 'exit' flag.
 *
 * Change History : 
 *
 * $Log: odomCommand.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "odometer.h"
#include "odomPlaylist.h"

class odomCmdInterp_t {
public:
   odomCmdInterp_t( odomPlaylist_t &playlist );
   ~odomCmdInterp_t( void );

   bool dispatch( char const *cmdline );

   char const *getErrorMsg( void ) const { return errorMsg_ ; }
   bool exitRequested( void ) const { return exit_ ; }

   odomPlaylist_t &playlist(){ return playlist_ ; }

private:
   odomCmdInterp_t( odomCmdInterp_t const & ); // no copies

   char inBuf_[512]; // temp space
   char errorMsg_[512];
   bool exit_ ;

   odomPlaylist_t &playlist_ ;
};

#endif

