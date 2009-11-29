/*
 * Module mplayerWrap.cpp
 *
 * This module defines the methods of the mplayerWrap_t
 * class as declared in mplayerWrap.h
 *
 * Change History : 
 *
 * $Log: mplayerWrap.cpp,v $
 * Revision 1.2  2007-08-23 00:31:32  ericn
 * -allow const parameters
 *
 * Revision 1.1  2007/08/14 12:59:26  ericn
 * -import
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

#include "mplayerWrap.h"
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

mplayerWrap_t::mplayerWrap_t
   ( char const *fileName,
     unsigned    x, 
     unsigned    y,
     unsigned    w,
     unsigned    h,
     char const **options,
     unsigned    numOptions )
   : fileName_( strdup( fileName ) )
   , childPid_( -1 )
{
   printf( "in mplayerWrap_t constructor\n" );
   int i ;
   int childStdin[2] = { -1, -1 };
   int childStdout[2] = { -1, -1 };
   int childStderr[2] = { -1, -1 };
   fds_[0] = fds_[1] = fds_[2] = -1 ;
   char geoParam[80];
   geoParam[0] = '\0' ;
   char const *const args[] = {
      "/bin/mplayer"
   ,  "-quiet"
   ,  "-geometry"
   ,  geoParam
   ,  "-framedrop"
   ,  "-slave"
   ,  fileName_
   };
   int const numFixedArgs = (sizeof(args)/sizeof(args[0]));
   printf( "%u fixed args\n", numFixedArgs );
   int totalArgs = numFixedArgs + numOptions + 1 ;
   char const **fixedupArgs = new char const *[totalArgs];
   for( i = 0 ; i < numFixedArgs ; i++ ){
      fixedupArgs[i] = args[i];
   }
   printf( "adding %u cmdline options\n", numOptions );
   for( ; (unsigned)i < numFixedArgs+numOptions ; i++ ){
      fixedupArgs[i] = options[i-numFixedArgs];
   }
   fixedupArgs[i] = 0 ;

   printf( "done fixing up options\n" );

   int rval = pipe( childStdin );
   if( rval )
      goto bail ;
   rval = pipe(childStdout);
   if( rval )
      goto bail ;
   rval = pipe(childStderr);
   if( rval )
      goto bail ;

   snprintf(geoParam,sizeof(geoParam), "%ux%u+%u+%u", w, h, x, y );
   
   childPid_ = fork();
   if( 0 == childPid_ ){
      printf( "child process: %d\n", getpid() );
      dup2(childStdin[0],fileno(stdin));
      dup2(childStdout[1],fileno(stdout));
      dup2(childStderr[1],fileno(stderr));

      // parent owns these
      close(childStdin[1]);
      close(childStdout[0]);
      close(childStderr[0]);

      for( i = 0 ; fixedupArgs[i]; i++ ){
         printf( "child param[%d] == %s\n", i, fixedupArgs[i] );
      }
      rval = execve( args[0], (char **)fixedupArgs, environ );
      printf( "execve: %m" );

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
   delete [] fixedupArgs ;
   return ;

bail:
   delete [] fixedupArgs ;
   for( i = 0 ; i < 2 ; i++ ){
      if( 0 <= childStdin[i] )
         close( childStdin[i] );
      if( 0 <= childStdout[i] )
         close( childStdout[i] );
      if( 0 <= childStderr[i] )
         close( childStderr[i] );
   }
}

void mplayerWrap_t::shutdown( void )
{
   for( int i = 0 ; i < 3 ; i++ )
      if( 0 <= fds_[i] )
         close( fds_[i] );
   if( 0 < childPid_ ){
      printf( "sending signal %d to pid %d\n", SIGTERM, childPid_ );
      kill(childPid_,SIGTERM);
   }
}

int mplayerWrap_t::wait( void )
{
   int exitStat ;
   pid_t pid = waitpid(childPid_, &exitStat, 0);
   if( pid == childPid_ ){
      childPid_ = 0 ;
   }
   return exitStat ;
}

mplayerWrap_t::~mplayerWrap_t( void )
{
   shutdown();
   if( fileName_ )
      free( fileName_ );
}

#ifdef STANDALONE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "fbDev.h"
#include "ttyPoll.h"
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
   if( 6 <= argc )
   {
      unsigned x, y, w, h ;
      unsigned *params[4] = { &x, &y, &w, &h };
      for( int i = 2 ; i < 6 ; i++ ){
         *params[i-2] = strtoul(argv[i],0,0);
      }

      mplayerWrap_t player( argv[1], x, y, w, h, argv+6, argc-6 );
      if( 0 <= player.pid() ){
         pollHandlerSet_t handlers ;
         myHandler_t childOut( player.child_stdout(), handlers );
         myHandler_t childErr( player.child_stderr(), handlers );
         ttyPollHandler_t tty( handlers );
         
         signal( SIGINT, ctrlcHandler );

         while( childOut.isOpen() && !die ){
            if( !handlers.poll(500) ){
            }
         }

         if( childOut.isOpen() ){
            player.shutdown();
         }

         if( 0 < player.pid() ){
            int exitStat = player.wait();
         }
      }
   }
   else
      fprintf( stderr, "Usage: %s fileName x y w h\n", argv[0] );

   return 0 ;
}

#endif
