#ifndef __ROLLINGMEDIAN_H__
#define __ROLLINGMEDIAN_H__ "$Id: rollingMedian.h,v 1.2 2004-12-03 04:42:46 ericn Exp $"

/*
 * rollingMedian.h
 *
 * This header file declares the rollingMedian_t
 * class, which is used to calculate and produce
 * median-of-n values for a stream of input data.
 *
 * It does this by keeping two arrays of input data:
 *    a circular buffer in input order
 *    a sorted array of the last N points
 *
 * Note that this class is optimized for small (<20)
 * values of N. 
 *
 * It expects unsigned short inputs, and will produce
 * at most 1 output for each input. It won't produce
 * any outputs until N points have been fed to it.
 *
 * Change History : 
 *
 * $Log: rollingMedian.h,v $
 * Revision 1.2  2004-12-03 04:42:46  ericn
 * -added dump method
 *
 * Revision 1.1  2004/11/26 15:30:04  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

class rollingMedian_t {
public:
   rollingMedian_t( unsigned N );
   ~rollingMedian_t( void );

   void feed( unsigned short value );

   // returns true and the median if at least N points 
   // have been fed
   bool read( unsigned short &median );

   // reset to initial value (use when stream stops)
   void reset( void );

   void dump( void ) const ;
private:
   unsigned               N_ ;
   unsigned               mid_ ;
   unsigned short *       circ_ ;
   unsigned short *       sorted_ ; 
   unsigned               numIn_ ;
};

#endif

