#ifndef __POPEN_H__
#define __POPEN_H__ "$Id: popen.h,v 1.1 2002-12-09 15:21:11 ericn Exp $"

/*
 * popen.h
 *
 * This header file declares the popen_t class, which is 
 * used to open a pipe to a child process.
 *
 *
 * Change History : 
 *
 * $Log: popen.h,v $
 * Revision 1.1  2002-12-09 15:21:11  ericn
 * -added module popen
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include <string>
#include <stdio.h>

class popen_t {
public:
   popen_t( char const *cmdLine );
   ~popen_t( void );

   // call this to verify process startup
   bool isOpen( void ) const { return 0 != fIn_ ; }

   // call this to retrieve a startup error message
   std::string getErrorMsg( void ) const { return errorMsg_ ; }

   //
   // call this to get the next line of text, or timeout
   // returns true if another line of text has become available
   //
   bool getLine( std::string &line,
                 unsigned     milliseconds );

private:
   FILE       *fIn_ ;
   std::string errorMsg_ ;
   std::string tempBuf_ ;
};

#endif

