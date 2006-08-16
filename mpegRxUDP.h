#ifndef __MPEGRXUDP_H__
#define __MPEGRXUDP_H__ "$Id: mpegRxUDP.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * mpegRxUDP.h
 *
 * This header file declares the mpegRxUDP_t class,
 * used to catch streaming MPEG data from a UDP port.
 *
 * This class will use non-blocking recv's to read data,
 * and will present one of two events for each received
 * packet:
 *
 *    onNewFile()    when the file changes
 *    onRx()         when new data comes in
 *
 * Change History : 
 *
 * $Log: mpegRxUDP.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "mpegUDP.h"

class mpegRxUDP_t {
public:
   mpegRxUDP_t( unsigned short portNum );
   virtual ~mpegRxUDP_t( void );

   bool isBound( void ) const { return 0 <= sFd_ ; }

   virtual void onNewFile( char const *fileName,
                           unsigned    fileNameLen );
   virtual void onRx( bool                 isVideo,
                      bool                 discont,
                      unsigned char const *fData,
                      unsigned             length,
                      long long            pts,
                      long long            dts );
   virtual void onEOF( char const   *fileName,
                       unsigned long totalBytes,
                       unsigned long videoBytes,
                       unsigned long audioBytes );

   // should be called periodically. 
   //
   // handler routines will be called out of here
   //
   void poll( void );

   int getFd( void ) const { return sFd_ ; }

private:
   void onEOF();

   int           sFd_ ;
   char          fileName_[MPEGUDP_MAXFILENAME];
   unsigned      prevId_ ;
   unsigned      prevSeq_ ;

   unsigned long totalBytes_ ;
   unsigned long videoBytes_ ;
   unsigned long audioBytes_ ;
};

#endif

