#ifndef __POLLHANDLER_H__
#define __POLLHANDLER_H__ "$Id: pollHandler.h,v 1.2 2003-11-02 17:58:05 ericn Exp $"

/*
 * pollHandler.h
 *
 * This header file declares the pollHandler_t class, and
 * the pollHandlerSet_t class, which are used to allow a 
 * single, generic thread to process I/O on a set of file
 * handles.
 *
 *
 * Change History : 
 *
 * $Log: pollHandler.h,v $
 * Revision 1.2  2003-11-02 17:58:05  ericn
 * -enhanced comment
 *
 * Revision 1.1  2003/10/05 19:15:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include <sys/poll.h>

class pollHandlerSet_t ;


class pollHandler_t {
public:
   pollHandler_t( int               fd,
                  pollHandlerSet_t &set );
   virtual ~pollHandler_t( void );

   //
   // These routines should return true if the handler
   // still wants callbacks on the same events.
   //
   virtual void onDataAvail( void );     // POLLIN
   virtual void onWriteSpace( void );    // POLLOUT
   virtual void onError( void );         // POLLERR
   virtual void onHUP( void );           // POLLHUP

   void setMask( short events );
   short getMask( void ) const { return mask_ ; }
   int   getFd( void ) const { return fd_ ; }

protected:
   int               fd_ ;
   pollHandlerSet_t &parent_ ;
   short             mask_ ;
};


class pollHandlerSet_t {
public:
   enum {
      maxHandlers_ = 8
   };

   pollHandlerSet_t( void );
   ~pollHandlerSet_t( void );

   void add( pollHandler_t &handler );
   void remove( pollHandler_t &handler );

   void setMask( pollHandler_t &handler, short newMask );

   //
   // returns true if at least one handler was notified, 
   // false if timed out
   //
   bool poll( int ms );

   unsigned numHandlers( void ) const { return numHandlers_ ; }
   pollHandler_t *operator[]( unsigned idx ) const { return handlers_[idx]; }

protected:
   unsigned          numHandlers_ ;
   pollHandler_t    *handlers_[maxHandlers_];
   pollfd            fds_[maxHandlers_];
};

#endif

