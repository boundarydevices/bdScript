/*
 * Module rawKbd.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: rawKbd.cpp,v $
 * Revision 1.1  2005-04-24 18:55:03  ericn
 * -Initial import (temporary file)
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */


#include "rawKbd.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

rawKbd_t :: rawKbd_t( int fd )
   : fd_( fd )
{
   tcgetattr( fd, &oldState_ );

   struct termios raw = oldState_ ;
   raw.c_lflag &=~ (ICANON | ECHO | ISIG );
   tcsetattr( fd, TCSANOW, &raw );
   fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | O_NONBLOCK);
}

rawKbd_t :: ~rawKbd_t( void )
{
   tcsetattr( fd_, TCSANOW, &oldState_ );
   fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) & ~O_NONBLOCK );
}

bool rawKbd_t :: read( char &c )
{
   fflush( stdout );
   return ( 1 == ::read( fd_, &c, 1 ) );
}

unsigned long rawKbd_t :: readln
   ( char    *data,        // output line goes here (NULL terminated)
     unsigned max,         // input : sizeof( databuf )
     char    *terminator ) // supply a location if you care
{
   if( terminator ) *terminator = '?' ;

   if( 0 < max ) {
           unsigned len = 0 ;
           bool terminated = false ;
           do {
                   char c ;
                   if( read( c ) )
                   {
                           switch( c )
                           {
                                   case '\x03' :
                                   case '\r' :
                                   case '\n' :
                                   case '\x1b' :
                                   {
                                           write( 1, "\r\n", 2 );
                                           terminated = true ;
                                           if( terminator ) *terminator = c ;
                                           break;
                                   }
                                   case '\b' :
                                   case '\x7f' :
                                   {
                                           if( 0 < len ) {
                                                   write( 1, "\b \b", 3 );
                                                   --len ;
                                           } else write( 1, "\x7", 1 );
                                           break;
                                   }
                                   default:
                                   {
                                           if( len < max-1 ) {
                                                   write( 1, &c, 1 );
                                                   data[len++] = c ;
                                           } else write( 1, "\x7", 1 );
                                   }
                           }
                   } // have another char
           } while( !terminated );

           data[len] = 0 ;
           return len ;
   } else {
           // ?? can't terminate
           return 0 ;
   }

}


#ifdef __MODULETEST__

int main( void )
{
   printf( "in rawKbdMain\n" );
   rawKbd_t kbd ;

   while( 1 )
   {
      printf( "cmd: " ); fflush( stdout );

      char inBuf[80];
      char term ;
      if( 0 < kbd.readln( inBuf, sizeof( inBuf ), &term ) )
      {
         printf( "<%s>\n", inBuf );
      }

      if( ( '\x03' == term ) || ( '\x1b' == term ) )
      {
         break;
      }
   }

   return 0 ;
}

#endif 
