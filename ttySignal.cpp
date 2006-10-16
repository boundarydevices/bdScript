/*
 * Module ttySignal.cpp
 *
 * This module defines the methods of the ttySignal_t
 * class as declared in ttySignal.h
 *
 * Change History : 
 *
 * $Log: ttySignal.cpp,v $
 * Revision 1.1  2006-10-16 22:45:50  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "ttySignal.h"
#include <fcntl.h>
#include "rtSignal.h"
#include "multiSignal.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "debugPrint.h"

#define STDINFD 0

static int ttySignal_ = 0 ;

void ttySignalHandler( int signo, void *param)
{
   debugPrint( "ttySignalHandler\n" );
   ttySignal_t *dev = (ttySignal_t *)param ;
   dev->rxSignal();
}

ttySignal_t::ttySignal_t( ttySignalHandler_t h )
: handler_( h )
{
   int rval = tcgetattr(STDINFD,&oldState_);
   if( 0 == rval ){
      struct termios newState = oldState_ ;
      cfmakeraw( &newState );
      rval = tcsetattr(STDINFD, TCSANOW, &newState);
      if( 0 == rval ){
         if( 0 == ttySignal_ ){
            ttySignal_ = nextRtSignal();
         }
         fcntl(STDINFD, F_SETOWN, getpid());
         fcntl(STDINFD, F_SETSIG, ttySignal_ );
         sigset_t blockThese ;
         sigemptyset( &blockThese );
         setSignalHandler( ttySignal_, blockThese, ttySignalHandler, this );
         if( 0 != handler_ ){
            int flags = flags = fcntl( STDINFD, F_GETFL, 0 );
            flags |= FASYNC ;
            fcntl( STDINFD, F_SETFL, flags | O_NONBLOCK );
            printf( "handler installed\n" );
         }
         else
            printf( "No handler installed\n" );
      }
      else {
         perror( "tcsetattr" );
      }
   }
   else
      perror( "tcgetattr" );
}

ttySignal_t::~ttySignal_t( void )
{
   tcsetattr(STDINFD, TCSANOW, &oldState_);
}

void ttySignal_t::setHandler( ttySignalHandler_t h )
{
   handler_ = h ;
}

void ttySignal_t::rxSignal( void )
{
   if( handler_ ){
      handler_( *this );
   }
   else {
      fprintf( stderr, "tty rx no handler\n" );
      fcntl( STDINFD, F_SETFL, fcntl( STDINFD, F_GETFL, 0 ) & ~FASYNC );
   }
}


#ifdef MODULETEST

#include <ctype.h>

static bool volatile die = false ;

static void handler( ttySignal_t &dev )
{
   printf( "h" );
   int numRead ;
   char inBuf[80];

   while( 0 < ( numRead = read( STDINFD, inBuf, sizeof(inBuf) ) ) ){
      for( int j = 0 ; j < numRead ; j++ )
      {
         char const c = inBuf[j];
         if( isprint(c) )
            printf( "%c", c );
         else
            printf( "<%02x>", (unsigned char)c );
         if( '\x03' == c ){
            printf( "\r\ndie\r\n" );
            die = true ;
         }
      }
      fflush(stdout);
   }
}

int main( void ){
   ttySignal_t tty(handler);
   printf( "Hit <Ctrl-C> to exit\n" );
   
   while( !die ){
      pause();
      printf( "w" ); fflush(stdout);
   }

   return 0 ;
}

#endif
