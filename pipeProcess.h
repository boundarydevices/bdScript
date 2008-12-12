#ifndef __PIPEPROCESS_H__
#define __PIPEPROCESS_H__ "$Id: pipeProcess.h,v 1.1 2008-12-12 01:24:47 ericn Exp $"

/*
 * pipeProcess.h
 *
 * This header file declares the pipeProcess_t class, which
 * wraps a sub-process with its stdin, stdout, and stderr 
 * re-directed to pipes.
 *
 * Copyright Boundary Devices, Inc. 2008
 */

class pipeProcess_t {
public:
   pipeProcess_t( char const *args[] ); // a.la. execve
   virtual ~pipeProcess_t( void );

   inline int pid( void ){ return childPid_ ; }
   inline int child_stdin( void ){ return fds_[0]; }
   inline int child_stdout( void ){ return fds_[1]; }
   inline int child_stderr( void ){ return fds_[2]; }

   inline int fdNum(int idx) const { return (idx < 3 ) ? fds_[idx] : -1 ; }
   bool isAlive(void) const ;

   void shutdown( void );
   int wait( void );

protected:
   char    *const progName_ ;
   int            childPid_ ;
   int            fds_[3]; // stdin/stdout/stderr
};

#endif

