/*
 * Module childProcess.cpp
 *
 * This module defines the methods of the childProcess_t
 * class as declared in childProcess.h
 *
 *
 * Change History : 
 *
 * $Log: childProcess.cpp,v $
 * Revision 1.3  2003-08-24 15:47:46  ericn
 * -exposed child process map
 *
 * Revision 1.2  2003/08/24 14:11:32  ericn
 * -fixed exec-error problem
 *
 * Revision 1.1  2002/10/25 02:55:01  ericn
 * -initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "childProcess.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/errno.h>


static pidToChild_t children_ ;
static struct sigaction oldint_ ;

static void childHandler
   ( int             sig, 
     struct siginfo *info, 
     void           *context )
{
   if( SIGCHLD == sig )
   {
      int exitCode ;
      struct rusage usage ;

      int pid ;
      while( 0 < ( pid = wait3( &exitCode, WNOHANG, &usage ) ) )
      {
         pidToChild_t :: iterator it = children_.find( pid );
         if( it != children_.end() )
         {
            childProcess_t *process = (*it).second ;
            children_.erase( it );
            process->died();
         }
//         else
//            printf( "unknown child process %d\n", pid );
      }
   }
}
 

void startChildMonitor() // start trapping SIGCHLD signal
{
   struct sigaction sa;
 
   /* Initialize the sa structure */
   sa.sa_handler = (__sighandler_t)childHandler ;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_SIGINFO ; // pass info
 
//   int result = 
   sigaction( SIGCHLD, &sa, &oldint_ );
//   printf( "established signal handler, result %d\n" );
}

void stopChildMonitor()  // stop trapping SIGCHLD signal
{
   sigaction( SIGCHLD, &oldint_, 0 );
}


bool childProcess_t :: run
   ( char const *path,
     char       *argv[],
     char       *envp[] )
{
   //
   // hold off on signals until we update the map
   //
   childProcessLock_t lock ;

   int childPid = fork();
   if( 0 == childPid )
   {
      execve( path, argv, envp );
      perror( path );
      exit(errno);
   } // child
   else if( 0 < childPid )
   {
      pid_ = childPid ;
      children_[pid_] = this ;
   } // parent, succeeded

   return 0 <= pid_ ;
}

void childProcess_t :: died( void )
{
   printf( "pid %d died\n", pid_ );
   pid_ = -1 ;
}

childProcessLock_t :: childProcessLock_t( void )
{
   sigemptyset( &mask_ );
   sigaddset( &mask_, SIGCHLD );
   sigprocmask( SIG_BLOCK, &mask_, 0 );
}

childProcessLock_t :: ~childProcessLock_t( void )
{
   sigemptyset( &mask_ );
   sigaddset( &mask_, SIGCHLD );
   sigprocmask( SIG_UNBLOCK, &mask_, 0 );
}

pidToChild_t const &getProcessMap( childProcessLock_t & )
{
   return children_ ;
}

#ifdef __MODULETEST__

int main( int argc, char **argv )
{
   if( 1 < argc )
   {
      startChildMonitor(); // start trapping SIGCHLD signal
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         char *cmd = argv[arg];
         char *args[2] = { cmd, 0 };
         childProcess_t *newChild = new childProcess_t ;
         if( newChild->run( cmd, args, __environ ) )
            printf( "ran child %d:%s\n", newChild->pid_, cmd );
         else
            printf( "error starting %s\n", cmd );
      }

      printf( "waiting for process completion\n" );

      do {
         {
            childProcessLock_t lock ;
            pidToChild_t const &children = getProcessMap( lock );
            if( 0 == children.size() )
               break;
         }
         
         // only get here if children are alive
         pause();

      } while( 1 );
      
      printf( "process complete\n" );
      
      stopChildMonitor();  // stop trapping SIGCHLD signal
   }
   else
      fprintf( stderr, "Usage : childProcess cmdline [cmdline...]\n" );

   return 0 ;

}

#endif
