#ifndef __ROLLINGMEAN_H__
#define __ROLLINGMEAN_H__ "$Id: rollingMean.h,v 1.1 2004-11-26 15:30:04 ericn Exp $"

/*
 * rollingMean.h
 *
 * This header file declares the rollingMean_t class, which is 
 * a filter that returns the average-of-N samples from a stream.
 *
 *
 * Change History : 
 *
 * $Log: rollingMean.h,v $
 * Revision 1.1  2004-11-26 15:30:04  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

class rollingMean_t {
public:
   rollingMean_t( unsigned N );
   ~rollingMean_t( void );

   void feed( unsigned short sample );

   bool read( unsigned short &mean );

   void reset( void );

private:
   unsigned               N_ ;
   unsigned short * const circ_ ;
   unsigned               numIn_ ;
   unsigned long          sum_ ;
};

#endif

