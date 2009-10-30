/*
 * Module pipeProcess.cpp
 *
 * This module defines the methods of the pipeProcess_t
 * class as declared in pipeProcess.h
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "pipeProcess.h"
#include "openFds.h"
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// #define DEBUGPRINT
#include "debugPrint.h"

pipeProcess_t::pipeProcess_t
   ( char const *args[] )
   : progName_( strdup( args[0] ) )
   , childPid_( -1 )
{
   int i ;
   int childStdin[2] = { -1, -1 };
   int childStdout[2] = { -1, -1 };
   int childStderr[2] = { -1, -1 };
   fds_[0] = fds_[1] = fds_[2] = -1 ;

   int rval = pipe( childStdin );
   if( rval )
      goto bail ;
   rval = pipe(childStdout);
   if( rval )
      goto bail ;
   rval = pipe(childStderr);
   if( rval )
      goto bail ;

   childPid_ = fork();
   if( 0 == childPid_ ){
      dup2(childStdin[0],fileno(stdin));
      dup2(childStdout[1],fileno(stdout));
      dup2(childStderr[1],fileno(stderr));

      // Close everything except stdin/stdout/stderr
      openFds_t openFds ;
      for( unsigned i = 0 ; i < openFds.count(); i++ ){
         if( 3 <= openFds[i] )
            close( openFds[i] );
      }

      rval = execve( args[0], (char **)args, environ );
      exit(0);
   }
   else {
      // child owns these
      close(childStdin[0]);
      close(childStdout[1]);
      close(childStderr[1]);
      fds_[0] = childStdin[1];
      fds_[1] = childStdout[0];
      fds_[2] = childStderr[0];
      for( i = 0 ; i <= 2 ; i++ ){
         fcntl(fds_[i], F_SETFD, FD_CLOEXEC );
         fcntl(fds_[i], F_SETFL, O_NONBLOCK );
      }
   }
   return ;

bail:
   for( i = 0 ; i < 2 ; i++ ){
      if( 0 <= childStdin[i] )
         close( childStdin[i] );
      if( 0 <= childStdout[i] )
         close( childStdout[i] );
      if( 0 <= childStderr[i] )
         close( childStderr[i] );
   }
}

void pipeProcess_t::shutdown( void )
{
   for( int i = 0 ; i < 3 ; i++ ){
      if( 0 <= fds_[i] ){
         close( fds_[i] );
         fds_[i] = -1 ;
      }
   }
   if( 0 < childPid_ ){
      kill(childPid_,SIGTERM);
      childPid_ = -1 ;
   }
   debugPrint( "~%s\n", __PRETTY_FUNCTION__ );
}

bool pipeProcess_t::isAlive(void) const 
{
   int rval = (0 < childPid_) ? kill(childPid_,SIGCONT) : -1 ;
   return ( 0 == rval );
}

int pipeProcess_t::wait( void )
{
   int exitStat ;
   if( 0 <= childPid_ ){
      pid_t pid = waitpid(childPid_, &exitStat, 0);
      if( pid == childPid_ ){
         childPid_ = 0 ;
      }
   }
   return exitStat ;
}

pipeProcess_t::~pipeProcess_t( void )
{
   shutdown();
   if( progName_ )
      free( progName_ );
}

#ifdef STANDALONE_CHILDPROCESS
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "fbDev.h"
#include "pollHandler.h"

static bool die = false ;
static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   die = true ;
}

class myHandler_t : public pollHandler_t {
public:
   myHandler_t( int fd, pollHandlerSet_t &set )
      : pollHandler_t( fd, set )
   {
      setMask( POLLIN|POLLERR|POLLHUP );
      set.add( *this );
   }
   virtual ~myHandler_t( void ){}

   virtual void onDataAvail( void );     // POLLIN
   virtual void onError( void );     // POLLIN
   virtual void onHUP( void );     // POLLIN
};

void myHandler_t :: onDataAvail( void )
{
   char inBuf[256];
   int numRead ; 
   while( 0 < (numRead = read( fd_, inBuf, sizeof(inBuf) )) )
   {
      printf( "rx <" ); fwrite( inBuf, numRead, 1, stdout ); printf( ">\n" );
   }
   if( 0 == numRead )
      onHUP();
}

void myHandler_t :: onError( void )         // POLLERR
{
   printf( "<Error>\n" );
}

void myHandler_t :: onHUP( void )           // POLLHUP
{
   printf( "<HUP>\n" );
   close();
}

int main( int argc, char const **argv )
{
   if( 1 < argc )
   {
      pipeProcess_t child( argv + 1 );
      if( 0 <= child.pid() ){
         pollHandlerSet_t handlers ;
         myHandler_t childOut( child.child_stdout(), handlers );
         myHandler_t childErr( child.child_stderr(), handlers );

         signal( SIGINT, ctrlcHandler );

         while( childOut.isOpen() && !die ){
            if( !handlers.poll(500) ){
            }
         }

         if( childOut.isOpen() ){
            child.shutdown();
         }

         if( 0 < child.pid() ){
            int exitStat = child.wait();
         }
      }
   }
   else
      fprintf( stderr, "Usage: %s fileName x y w h\n", argv[0] );

   return 0 ;
}

#endif
