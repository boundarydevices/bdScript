#ifndef __POLLHANDLER_H__
#define __POLLHANDLER_H__ "$Id: pollHandler.h,v 1.8 2004-02-08 10:34:46 ericn Exp $"

/*
 * pollHandler.h
 *
 * This header file declares the pollHandler_t, pollClient_t,
 * and pollHandlerSet_t classes, which are used to allow a 
 * single, generic thread to process I/O on a set of file
 * handles.
 *
 * Expected usage is something like the following:
 *
 *       pollHandlerSet_t handlers ;
 *
 *       ...instantiate some handlers here
 *
 *       while( !exit )
 *          handlers.poll( -1UL );
 *
 * Various pollHandler_t variants are available for serial
 * ports, console, barcode readers, etc...
 *
 * Usually, application-level processing is done directly by
 * pollHandler_t descendants, but the pollClient_t provides 
 * an extra level of indirection when it is useful to re-use
 * an I/O channel for multiple requests (e.g. Audio, HTTP, HTTPS).
 * 
 *
 * Change History : 
 *
 * $Log: pollHandler.h,v $
 * Revision 1.8  2004-02-08 10:34:46  ericn
 * -added pollClient_t declaration
 *
 * Revision 1.7  2004/01/01 20:11:42  ericn
 * -added isOpen() routine, and switched pollHandlers to use close()
 *
 * Revision 1.6  2004/01/01 16:02:15  ericn
 * -added close() method
 *
 * Revision 1.5  2003/12/28 20:54:35  ericn
 * -prevent copies
 *
 * Revision 1.4  2003/11/28 14:08:34  ericn
 * -added getter method isDeleted()
 *
 * Revision 1.3  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.2  2003/11/02 17:58:05  ericn
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
#include <unistd.h>

class pollHandlerSet_t ;

class pollHandler_t {
public:
   pollHandler_t( int               fd,
                  pollHandlerSet_t &set );
   virtual ~pollHandler_t( void );

   virtual void onDataAvail( void );     // POLLIN
   virtual void onWriteSpace( void );    // POLLOUT
   virtual void onError( void );         // POLLERR
   virtual void onHUP( void );           // POLLHUP

   void  setMask( short events );
   short getMask( void ) const { return mask_ ; }
   int   getFd( void ) const { return fd_ ; }
   bool  isOpen( void ) const { return 0 <= fd_ ; }
   void  close( void ){ ::close( fd_ ); fd_ = -1 ; }

protected:
   int               fd_ ;
   pollHandlerSet_t &parent_ ;
   short             mask_ ;
private:
   pollHandler_t( pollHandler_t const & ); // no copies
};


class pollClient_t {
public:
   virtual ~pollClient_t( void );

   virtual unsigned short getMask( void );
   virtual void onDataAvail( pollHandler_t & );     // POLLIN
   virtual void onWriteSpace( pollHandler_t & );    // POLLOUT
   virtual void onError( pollHandler_t & );         // POLLERR
   virtual void onHUP( pollHandler_t & );           // POLLHUP
};



class pollHandlerSet_t {
public:
   enum {
      maxHandlers_ = 8
   };

   pollHandlerSet_t( void );
   ~pollHandlerSet_t( void );

   void add( pollHandler_t &handler );
   void removeMe( pollHandler_t &handler );

   void setMask( pollHandler_t &handler, short newMask );

   //
   // returns true if at least one handler was notified, 
   // false if timed out
   //
   bool poll( int ms );

   unsigned numHandlers( void ) const { return numHandlers_ ; }
   pollHandler_t *operator[]( unsigned idx ) const { return handlers_[idx]; }
   
   bool isDeleted( unsigned idx ) const { return deleted_[idx]; }

protected:
   bool              inPollLoop_ ;
   unsigned          numHandlers_ ;
   pollHandler_t    *handlers_[maxHandlers_];
   pollfd            fds_[maxHandlers_];
   bool              deleted_[maxHandlers_];
private:
   pollHandlerSet_t( pollHandlerSet_t const & ); // no copies
};

#endif

