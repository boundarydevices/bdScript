#ifndef __USBLPPOLL_H__
#define __USBLPPOLL_H__ "$Id: usblpPoll.h,v 1.2 2007-01-03 22:00:04 ericn Exp $"

/*
 * usblpPoll.h
 *
 * This header file declares the usblbPoll_t class,
 * which provides bi-directional communication with 
 * a USB line printer device.
 *
 * Both input and output are buffered.
 *
 * Change History : 
 *
 * $Log: usblpPoll.h,v $
 * Revision 1.2  2007-01-03 22:00:04  ericn
 * -made log file public
 *
 * Revision 1.1  2006/10/29 21:59:10  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#ifndef __POLLHANDLER_H__
#include "pollHandler.h"
#endif

#include <stdio.h>

#define DEFAULT_USBLB_DEV "/dev/usb/lp0"

class usblpPoll_t : public pollHandler_t {
public:
   usblpPoll_t( pollHandlerSet_t &set,
                char const       *devName = DEFAULT_USBLB_DEV,
                unsigned          outBuffer = 32768,
                unsigned          inBuffer = 4096 );
   ~usblpPoll_t( void );

   // override this to perform processing of received data
   // This callback will be made after 
   virtual void onDataIn( void );
   
   //    read (and dequeue) data up to max bytes
   //    returns false when nothing is present
   bool read( char *inData, unsigned max, unsigned &len );
   bool haveData( void ) const { return inTake_ != inAdd_ ; }
   unsigned bytesAvail( void ) const ;

   // general pollHandler input routines (don't override)
   virtual void onDataAvail( void );     // POLLIN
   virtual void onWriteSpace( void );    // POLLOUT

   // returns number of bytes queued.
   int write( void const *data, int length );

public:
   unsigned const inBufferLength_ ;
   unsigned       inAdd_ ;
   unsigned       inTake_ ;
   char * const   inData_ ;
   unsigned const outBufferLength_ ;
   unsigned       outAdd_ ;
   unsigned       outTake_ ;
   char * const   outData_ ;
public:
   FILE          *fLog_ ;
};

#endif

