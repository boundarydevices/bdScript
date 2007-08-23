#ifndef __MPLAYERWRAP_H__
#define __MPLAYERWRAP_H__ "$Id: mplayerWrap.h,v 1.2 2007-08-23 00:31:32 ericn Exp $"

/*
 * mplayerWrap.h
 *
 * This header file declares the mplayerWrap_t class,
 * which will start up an MPlayer sub-process with the
 * specified video file as a parameter.
 *
 *
 * Change History : 
 *
 * $Log: mplayerWrap.h,v $
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

class mplayerWrap_t {
public:
   mplayerWrap_t( char const  *fileName,
                  unsigned     x, 
                  unsigned     y,
                  unsigned     w,
                  unsigned     h,
                  char const **options = 0,
                  unsigned     numOptions = 0 );
   virtual ~mplayerWrap_t( void );

   inline int pid( void ){ return childPid_ ; }
   inline int child_stdin( void ){ return fds_[0]; }
   inline int child_stdout( void ){ return fds_[1]; }
   inline int child_stderr( void ){ return fds_[2]; }

   void shutdown( void );
   int wait( void );
protected:
   char    *const fileName_ ;
   int            childPid_ ;
   int            fds_[3]; // stdin/stdout/stderr
};

#endif

