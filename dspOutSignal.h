#ifndef __DSPOUTSIGNAL_H__
#define __DSPOUTSIGNAL_H__ "$Id: dspOutSignal.h,v 1.1 2006-10-16 22:45:30 ericn Exp $"

/*
 * dspOutSignal.h
 *
 * This header file declares the dspOutSignal_t
 * class, which is a wrapper class that opens an
 * output channel to /dev/dsp and attaches it to
 * a real-time signal.
 *
 * Note that this class sets up an fd for real-time signals, but
 * does not enable the signal until the enable() routine is called
 * to allow a handler to be installed first.
 * 
 * Change History : 
 *
 * $Log: dspOutSignal.h,v $
 * Revision 1.1  2006-10-16 22:45:30  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class dspOutSignal_t {
public:
   dspOutSignal_t( void );
   ~dspOutSignal_t( void );

   inline bool isOpen( void ) const { return 0 <= fd_ ; }
   inline int getFd( void ) const { return fd_ ; }
   inline int getSignal( void ) const { return sig_ ; }

   void enable();
   void disable();

private:
   int   fd_ ;
   int   sig_ ;
};


#endif

