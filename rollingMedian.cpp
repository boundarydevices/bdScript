/*
 * Module rollingMedian.cpp
 *
 * This module defines the methods of the
 * rollingMedian_t class as declared in rollingMedian.h
 *
 *
 * Change History : 
 *
 * $Log: rollingMedian.cpp,v $
 * Revision 1.1  2004-11-26 15:30:04  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "rollingMedian.h"
#include <stdlib.h>

rollingMedian_t :: rollingMedian_t( unsigned N )
   : N_( N )
   , mid_( N/2 )
   , circ_( new unsigned short [N] )
   , sorted_( new unsigned short [N] )
   , numIn_( 0 )
{
}

rollingMedian_t :: ~rollingMedian_t( void )
{
   delete [] circ_ ;
   delete [] sorted_ ;
}

static int compareShorts(const void *lhp, const void *rhp)
{
   unsigned short const lh = *(unsigned short const *)lhp ;
   unsigned short const rh = *(unsigned short const *)rhp ;
   return lh-rh ;
}

void rollingMedian_t :: feed( unsigned short newValue )
{
   unsigned const circPos = numIn_ % N_ ;
   unsigned short const oldValue = circ_[circPos];
   circ_[circPos] = newValue ;
   ++numIn_ ;

   if( N_ < numIn_ )
   {
      // binary search for old value
      // 
      // during the search
      //    lbound is <= pos
      //    rbound is > pos
      //
      // completes when sorted_[lbound] == oldest
      //
      unsigned lbound ;
      unsigned rbound ;
      unsigned short const median = sorted_[mid_];
      if( oldValue < median )
      {
         lbound = 0 ;
         rbound = mid_ ;
      }
      else
      {
         lbound = mid_ ;
         rbound = N_ ;
      }
      
      while( lbound < rbound )
      {
         unsigned const midPos = lbound+((rbound-lbound) >> 1 );
         int diff = oldValue - sorted_[midPos];
         if( 0 == diff )
         {
            lbound = rbound = midPos ;
         }
         else if( 0 < diff )
         {
            lbound = midPos+1 ;
         }
         else
         {
            rbound = midPos ;
         }
      }

      unsigned oldPos = lbound ;

      //
      // binary search for new value insert position
      //
      // notes above about bounds apply here, too
      //
      if( newValue < median )
      {
         lbound = 0 ;
         rbound = mid_ ;
      }
      else
      {
         lbound = mid_ ;
         rbound = N_ ;
      }
      
      while( lbound < rbound )
      {
         unsigned const midPos = lbound+((rbound-lbound) >> 1 );
         int diff = newValue - sorted_[midPos];
         if( 0 == diff )
         {
            lbound = rbound = midPos ;
         }
         else if( 0 < diff )
         {
            lbound = midPos+1 ;
         }
         else
         {
            rbound = midPos ;
         }
      }
      unsigned const insertPos = ( lbound >= N_ )
                                 ? N_
                                 : lbound ;

      while( oldPos < insertPos )
      {
         sorted_[oldPos] = sorted_[oldPos+1];
         oldPos++;
      } // shift some items left into [oldPos..insertPos), then insert
      
      while( oldPos > insertPos )
      {
         sorted_[oldPos] = sorted_[oldPos-1];
         oldPos-- ;
      } // shift some items right into (insertPos..oldPos], then insert

      sorted_[oldPos] = newValue ; // woohoo!

   } // already sorted, remove oldest, insert newest
   else if( N_ == numIn_ )
   {
      memcpy( sorted_, circ_, N_*sizeof(sorted_[0]) );
      qsort( sorted_, N_, sizeof(sorted_[0]), compareShorts );
   }
   else
   {
   } // not enough inputs
}

bool rollingMedian_t :: read( unsigned short &median )
{
   if( N_ <= numIn_ )
   {
      median = sorted_[mid_];
      return true ;
   }

   return false ;
}

void rollingMedian_t :: reset( void )
{
   numIn_ = 0 ;
}

