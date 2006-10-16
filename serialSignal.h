#ifndef __SERIALSIGNAL_H__
#define __SERIALSIGNAL_H__ "$Id: serialSignal.h,v 1.1 2006-10-16 22:45:47 ericn Exp $"

/*
 * serialSignal.h
 *
 * This header file declares the serialSignal_t class,
 * which will open a serial port and establish a signal 
 * handler for receipt of data.
 *
 * The constructor sets the serial port to raw mode and 
 * non-blocking.
 *
 * Applications should use setSerial.cpp for baud, parity,
 * 
 * The handler itself defers reception to a call-back 
 * function. 
 *
 * No facilities are currently present for signalling
 * transmit space availability.
 *
 * Change History : 
 *
 * $Log: serialSignal.h,v $
 * Revision 1.1  2006-10-16 22:45:47  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class serialSignal_t ;

typedef void (*serialSignalHandler_t)( serialSignal_t &dev );

class serialSignal_t {
public:
   serialSignal_t( char const           *devName,
                   serialSignalHandler_t handler = 0 );
   ~serialSignal_t( void );

   inline bool isOpen( void ) const { return 0 <= fd_ ; }
   inline int getFd( void ) const { return fd_ ; }

   void setHandler( serialSignalHandler_t );
   inline void clearHandler( void ){ setHandler( 0 ); }

   // handler is called through this routine
   void rxSignal( void );

private:
   int                     fd_ ;
   serialSignalHandler_t   handler_ ; 
};

#endif

