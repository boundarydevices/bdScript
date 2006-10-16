#ifndef __BLOCKSIG_H__
#define __BLOCKSIG_H__ "$Id: blockSig.h,v 1.1 2006-10-16 22:45:28 ericn Exp $"

/*
 * blockSig.h
 *
 * This header file declares the blockSignal_t class, which is used 
 * to block signals during its' scope.
 *
 * Note that this class will not enable a signal in its' destructor
 * unless it was enabled to begin with.
 *
 * Change History : 
 *
 * $Log: blockSig.h,v $
 * Revision 1.1  2006-10-16 22:45:28  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class blockSignal_t {
public:
   blockSignal_t( int signo );
   ~blockSignal_t( void );

   int  sig_ ;
   bool wasMasked_ ;
};

#endif

