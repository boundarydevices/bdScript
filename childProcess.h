#ifndef __CHILDPROCESS_H__
#define __CHILDPROCESS_H__ "$Id: childProcess.h,v 1.2 2003-08-24 15:47:43 ericn Exp $"

/*
 * childProcess.h
 *
 * This header file declares the childProcess_t class,
 * which is an abstract base class wrapper for child 
 * processes with a specific set of characteristics:
 *
 *    No inherited file handles.
 *    Asynchronous notification of completion
 *
 * Essentially, this is just a wrapper for the sigaction() call,
 * which keeps track of some context for individual child processes.
 *
 * This allows, for example, a different call-back when an MP3 play
 * operation completes than for completion of an animation.
 *
 * Note that call-backs 
 *
 * Change History : 
 *
 * $Log: childProcess.h,v $
 * Revision 1.2  2003-08-24 15:47:43  ericn
 * -exposed child process map
 *
 * Revision 1.1  2002/10/25 02:55:01  ericn
 * -initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <signal.h>
#include <map>

void startChildMonitor(); // start trapping SIGCHLD signal
void stopChildMonitor();  // stop trapping SIGCHLD signal


struct childProcess_t {
   childProcess_t( void )
      : pid_( -1 ){}
   ~childProcess_t( void ){}

   bool run( char const *path,
             char       *argv[],
             char       *envp[] );

   //
   // override this to get process-specific handling
   //
   virtual void died( void );
   bool         isRunning( void ) const { return 0 < pid_ ; }

   int pid_ ;
};

typedef std::map<int,childProcess_t *> pidToChild_t ;

//
// use this class to temporarily block the SIGCHLD
// signal handler (while updating shared structures)
//
class childProcessLock_t {
public:
   childProcessLock_t( void );
   ~childProcessLock_t( void );
private:
   sigset_t mask_ ;
};

//
// use this to get the process map. 
//
// Note that you must hold a lock while accessing it.
//
pidToChild_t const &getProcessMap( childProcessLock_t & );

#endif

