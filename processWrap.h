#ifndef __PROCESSWRAP_H__
#define __PROCESSWRAP_H__ "$Id: processWrap.h,v 1.1 2008-06-06 21:48:10 ericn Exp $"

/*
 * processWrap.h
 *
 * This header file declares the processWrap_t class,
 * which will start up a sub-process with the specified 
 * parameters and provide pipes to its' stdin, stdout,
 * and stderr.
 *
 *
 * Change History : 
 *
 * $Log: processWrap.h,v $
 * Revision 1.1  2008-06-06 21:48:10  ericn
 * -import
 *
 * Revision 1.2  2007-08-23 00:31:32  ericn
 * -allow const parameters
 *
 * Revision 1.1  2007/08/14 12:59:24  ericn
 * -import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */

class processWrap_t {
public:
   processWrap_t( const char *path, 
                  char *const argv[]); // see execv for details
   virtual ~processWrap_t( void );

   inline int pid( void ){ return childPid_ ; }
   inline int child_stdin( void ){ return fds_[0]; }
   inline int child_stdout( void ){ return fds_[1]; }
   inline int child_stderr( void ){ return fds_[2]; }

   void shutdown( void );
   int wait( void );
protected:
   int            childPid_ ;
   int            fds_[3]; // stdin/stdout/stderr
};

#endif

