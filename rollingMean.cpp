/*
 * Module rollingMean.cpp
 *
 * This module defines the methods of the
 * rollingMean_t class as declared in rollingMean.h
 *
 *
 * Change History : 
 *
 * $Log: rollingMean.cpp,v $
 * Revision 1.1  2004-11-26 15:30:04  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "rollingMean.h"

rollingMean_t :: rollingMean_t( unsigned N )
   : N_( N )
   , circ_( new unsigned short[ N ] )
   , numIn_( 0 )
   , sum_( 0 )
{
}

rollingMean_t :: ~rollingMean_t( void )
{
   delete [] circ_ ;
}

void rollingMean_t :: feed( unsigned short sample )
{
   sum_ += sample ;

   unsigned const circPos = numIn_ % N_ ;
   numIn_++ ;
   unsigned short const oldValue = circ_[circPos];
   circ_[circPos] = sample ;

   if( numIn_ > N_ )
      sum_ -= oldValue ;
}

bool rollingMean_t :: read( unsigned short &mean )
{
   mean = sum_ / N_ ;
   return numIn_ >= N_ ;
}

void rollingMean_t :: reset( void )
{
   numIn_ = 0 ; 
   sum_   = 0 ;
}

