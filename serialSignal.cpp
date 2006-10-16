/*
 * Module serialSignal.cpp
 *
 * This module defines the methods of the serialSignal_t
 * class as declared in serialSignal.h
 *
 *
 * Change History : 
 *
 * $Log: serialSignal.cpp,v $
 * Revision 1.1  2006-10-16 22:45:46  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "serialSignal.h"
#include "multiSignal.h"
#include "rtSignal.h"
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include "setSerial.h"

// #define DEBUGPRINT
#include "debugPrint.h"

static int serialSignal_ = 0 ;

void serialSignalHandler( int signo, void *param)
{
   debugPrint( "serialSignalHandler\n" );
   serialSignal_t *dev = (serialSignal_t *)param ;
   dev->rxSignal();
}

serialSignal_t::serialSignal_t
   ( char const           *devName,
     serialSignalHandler_t handler )
   : fd_( open( devName, O_RDWR ) )
   , handler_( handler )
{
   if( isOpen() ){
      setRaw( fd_ );
      if( 0 == serialSignal_ ){
         serialSignal_ = nextRtSignal();
      }
      fcntl(fd_, F_SETOWN, getpid());
      fcntl(fd_, F_SETSIG, serialSignal_ );
      sigset_t blockThese ;
      sigemptyset( &blockThese );
      setSignalHandler( serialSignal_, blockThese, serialSignalHandler, this );
      if( 0 != handler_ ){
         int flags = flags = fcntl( fd_, F_GETFL, 0 );
         flags |= FASYNC ;
         fcntl( fd_, F_SETFL, flags | O_NONBLOCK );
      }
   }
}

serialSignal_t::~serialSignal_t( void )
{
   if( isOpen() ){
      close( fd_ );
      fd_ = -1 ;
      clearSignalHandler( serialSignal_, serialSignalHandler, this );
   }
}

void serialSignal_t::setHandler( serialSignalHandler_t h )
{
   handler_ = h ;
}

void serialSignal_t::rxSignal( void )
{
   if( handler_ ){
      handler_( *this );
   }
   else {
      fprintf( stderr, "serial rx no handler\n" );
      fcntl( fd_, F_SETFL, fcntl( fd_, F_GETFL, 0 ) & ~FASYNC );
   }
}

#ifdef MODULETEST
#include <ctype.h>
#include <stdlib.h>

static void handler( serialSignal_t &dev )
{
   int numRead ;
   char inBuf[80];

   while( 0 < ( numRead = read( dev.getFd(), inBuf, sizeof(inBuf) ) ) ){
      for( int j = 0 ; j < numRead ; j++ )
      {
         char const c = inBuf[j];
         if( isprint(c) )
            printf( "%c", c );
         else
            printf( "<%02x>", (unsigned char)c );
      }
      fflush(stdout);
   }
}

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      char const *const deviceName = argv[1];
      serialSignal_t ss( deviceName, handler );
      if( ss.isOpen() )
      {
         unsigned baud = ( 2 < argc ) ? strtoul( argv[2], 0, 0 ) : 115200 ;
         int rval = setBaud( ss.getFd(), baud );
         if( 0 == rval )
            debugPrint( "set baud to %u\n", baud );
         else
            printf( "error setting baud to %u\n", baud );

         unsigned dataBits = ( 3 < argc ) ? strtoul( argv[3], 0, 0 ) : 8 ;
         rval = setDataBits( ss.getFd(), dataBits );
         if( 0 == rval )
            debugPrint( "set data bits to %u\n", dataBits );
         else
            printf( "error setting data bits to %u\n", dataBits );

         char parity = ( 4 < argc ) ? toupper( *argv[4] ) : 'N' ;
         rval = setParity( ss.getFd(), parity );
         if( 0 == rval )
            debugPrint( "set parity to %c\n", parity );
         else
            printf( "error setting parity to %c\n", parity );

         unsigned stopBits = ( ( 5 < argc ) && ( '2' == *argv[5] ) ) ? 2  : 1 ;
         rval = setStopBits( ss.getFd(), stopBits );
         if( 0 == rval )
            debugPrint( "set stop bits to %u\n", stopBits );
         else
            printf( "error setting stop bits to %u\n", stopBits );

         debugPrint( "device opened\n" );
         while( 1 ){
            pause();
         }
      }
      else
         perror( deviceName );
   }
   else
      fprintf( stderr, "Usage: serialSignal deviceName [baud=115200 [databits=8 [parity=N]]]\n" );

   return 0 ;
}
#endif
