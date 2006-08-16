/*
 * Module odomTTY.cpp
 *
 * This module defines the methods of the odomTTY_t
 * class as declared in odomTTY.h
 *
 *
 * Change History : 
 *
 * $Log: odomTTY.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomTTY.h"

odomTTY_t::odomTTY_t
   ( pollHandlerSet_t  &set,
     odomCmdInterp_t   &interp )
   : ttyPollHandler_t(set,0)
   , interp_( interp )
{
}

odomTTY_t::~odomTTY_t( void )
{
}

void odomTTY_t::onLineIn( void )
{
   if( !interp_.dispatch( getLine() ) ){
      fprintf( stderr, "%s: %s\n", getLine(), interp_.getErrorMsg() );
   }
}

void odomTTY_t::onCtrlC( void )
{
   ttyPollHandler_t::onCtrlC();
   interp_.dispatch( "exit" );
   printf( "<ctrl-c>\n" );
}

#ifdef MODULETEST
#include <stdio.h>
#include "odomPlaylist.h"
#include <execinfo.h>
#include "hexDump.h"
#include <signal.h>

static struct sigaction sa;
static struct sigaction oldint;

static void crashHandler(int sig) 
{
   fprintf( stderr, "got signal, stack == %p\n", &sig );
   fprintf( stderr, "sighandler at %p\n", crashHandler );

   unsigned long addr = (unsigned long)&sig ;
   unsigned long page = addr & ~0xFFF ; // 4K
   unsigned long size = page+0x1000-addr ;
   hexDumper_t dumpStack( &sig, size ); // just dump this page
   while( dumpStack.nextLine() )
      fprintf( stderr, "%s\n", dumpStack.getLine() );

   void *btArray[128];
   int const btSize = backtrace(btArray, sizeof(btArray) / sizeof(void *) );

   fprintf( stderr, "########## Backtrace ##########\n"
                    "Number of elements in backtrace: %u\n", btSize );

   if (btSize > 0)
      backtrace_symbols_fd( btArray, btSize, fileno(stdout) );

   fprintf( stderr, "Handler done.\n" );
   fflush( stderr );
   if( oldint.sa_handler )
      oldint.sa_handler( sig );

   exit( 1 );
}

int main( int argc, char const * const argv[] )
{
   sa.sa_handler = crashHandler;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   // Set up the signal handler
   sigaction(SIGSEGV, &sa, NULL);
   
   odomPlaylist_t  playlist ;
   odomCmdInterp_t interp( playlist );
   for( int arg = 1 ; arg < argc ; arg++ ){
      char const *cmd = argv[arg];
      printf( "%s: ", cmd );
      if( interp.dispatch( cmd ) )
         printf( "success\n" );
      else
         printf( "error %s\n", interp.getErrorMsg() );
   }
   
   pollHandlerSet_t handlers ;
   odomTTY_t tty( handlers, interp );

   odometerSet_t &odometers = odometerSet_t::get();
   
   odometers.run();

   while( !interp.exitRequested() )
   {
      playlist.play( odometers.syncCount() );
      handlers.poll( -1 );
   }

   return 0 ;
}

#endif                            
