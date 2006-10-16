#ifndef __VSYNCSIGNAL_H__
#define __VSYNCSIGNAL_H__ "$Id: vsyncSignal.h,v 1.1 2006-10-16 22:45:54 ericn Exp $"

/*
 * vsyncSignal.h
 *
 * This header file declares the vsyncSignal_t class, which 
 * opens a channel to the /dev/sm501vsync device, allocates
 * a signal number, and sets the owner and signal number on the fd.
 *
 * Applications should use the setSignalHandler() routine
 * in the multiSignal module to establish a handler.
 *
 * Note that this class sets up an fd for real-time signals, but
 * does not enable the signal until the enable() routine is called
 * to allow a handler to be installed first.
 * 
 * Change History : 
 *
 * $Log: vsyncSignal.h,v $
 * Revision 1.1  2006-10-16 22:45:54  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class vsyncSignal_t {
public:
   static vsyncSignal_t &get();

   inline bool isOpen( void ) const { return 0 <= fd_ ; }
   inline int getFd( void ) const { return fd_ ; }
   inline int getSignal( void ) const { return signal_ ; }

   void enable();
   void disable();

private:
   vsyncSignal_t( void );
   ~vsyncSignal_t( void );

   int      fd_ ;
   int      signal_ ;
};

#endif

