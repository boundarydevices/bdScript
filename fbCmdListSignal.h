#ifndef __FBCMDLISTSIGNAL_H__
#define __FBCMDLISTSIGNAL_H__ "$Id: fbCmdListSignal.h,v 1.1 2006-10-16 22:45:34 ericn Exp $"

/*
 * fbCmdListSignal.h
 *
 * This header file declares the fbCmdListSignal_t class,
 * which is used to control(share) access to the command-list
 * device and signal in an application.
 *
 * As with the vsyncSignal_t class, the application should
 * use the multiSignal interface for establishing a handler.
 *
 * Note that this class sets up an fd for real-time signals, but
 * does not enable the signal until the enable() routine is called
 * to allow a handler to be installed first.
 * 
 * Change History : 
 *
 * $Log: fbCmdListSignal.h,v $
 * Revision 1.1  2006-10-16 22:45:34  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class fbCmdListSignal_t {
public:
   static fbCmdListSignal_t &get();

   inline bool isOpen( void ) const { return 0 <= fd_ ; }
   inline int getFd( void ) const { return fd_ ; }
   inline int getSignal( void ) const { return signal_ ; }

   void enable();
   void disable();

private:
   fbCmdListSignal_t( void );
   ~fbCmdListSignal_t( void );

   int      fd_ ;
   int      signal_ ;
};

#endif

