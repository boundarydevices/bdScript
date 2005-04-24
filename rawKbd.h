#ifndef __RAWKBD_H__
#define __RAWKBD_H__ "$Id: rawKbd.h,v 1.1 2005-04-24 18:55:03 ericn Exp $"

/*
 * rawKbd.h
 *
 * This header file declares a rawKbd_t class
 * for use in apps which need access to individual
 * keystrokes and 
 *
 *
 * Change History : 
 *
 * $Log: rawKbd.h,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */

#include <termios.h>

class rawKbd_t {
public:
   rawKbd_t( int fd = 0 );
   ~rawKbd_t( void );

   // returns true and the character if present
   bool read( char &c );

   unsigned long readln( char    *data,             // output line goes here (NULL terminated)
                         unsigned max,              // input : sizeof( databuf )
                         char    *terminator = 0 ); // supply a location if you care

private:
   int const      fd_ ;
   struct termios oldState_ ;
};

#endif

