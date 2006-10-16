#ifndef __YUVSIGNAL_H__
#define __YUVSIGNAL_H__ "$Id: yuvSignal.h,v 1.1 2006-10-16 22:45:56 ericn Exp $"

/*
 * yuvSignal.h
 *
 * This header file declares the yuvSignal_t class, which
 * controls sharing of the yuv device handle and signal
 * for signal-driven applications.
 *
 * Signal handler(s) should be registered through the 
 * multiSignal module.
 *
 * Note that this class sets up an fd for real-time signals, but
 * does not enable the signal until the enable() routine is called
 * to allow a handler to be installed first.
 * 
 * Change History : 
 *
 * $Log: yuvSignal.h,v $
 * Revision 1.1  2006-10-16 22:45:56  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class yuvSignal_t {
public:
   static yuvSignal_t &get();

   inline bool isOpen( void ) const { return 0 <= fd_ ; }
   inline int getFd( void ) const { return fd_ ; }
   inline int getSignal( void ) const { return signal_ ; }

   void enable();
   void disable();

private:
   yuvSignal_t( void );
   ~yuvSignal_t( void );

   int      fd_ ;
   int      signal_ ;
};

#endif

