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
 * Revision 1.3  2005-08-12 03:31:46  ericn
 * -include <string.h>
 *
 * Revision 1.2  2004/12/03 04:43:16  ericn
 * -added dump method,standalone program, then fixed
 *
 * Revision 1.1  2004/11/26 15:30:04  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "rollingMedian.h"
#include <stdlib.h>
#include <stdio.h>
// #define DEBUGPRINT
#include "debugPrint.h"
#include <string.h>

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

   debugPrint( "replacing %u with %u at position %u\n", oldValue, newValue, circPos );

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
debugPrint( "  oldSearch: l->r %u->%u\n", lbound, rbound );
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
debugPrint( "  oldPos == %u\n", oldPos );

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
debugPrint( "  newSearch: l->r %u->%u\n", lbound, rbound );
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

      // adjust insertPos for missing oldPos
      unsigned insertPos = ( lbound > oldPos )
                           ? lbound - 1 
                           : lbound ;

debugPrint( "insertPos: %u, %u\n", insertPos, lbound );

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

void rollingMedian_t :: dump( void ) const 
{
   unsigned max ;
   unsigned start ;
   if( numIn_ > N_ )
   { 
      max = N_ ;
      start = numIn_ ;
   }
   else
   {
      max = numIn_ ;
      start = 0 ;
   }

   printf( "history:\n" );
   for( unsigned i = 0 ; i < max ; i++ )
   {
      unsigned pos = ( start + i ) % N_ ;
      printf( "   %u", circ_[pos] );
   }
   
   if( numIn_ >= N_ )
   {
      printf( "\nsorted:\n" );
      unsigned short prev = 0 ;
      for( unsigned i = 0 ; i < max ; i++ )
      {
         unsigned short next = sorted_[i];
         printf( "   %u", next );
         if( next < prev )
            printf( "!!!!!" );
         prev = next ;
      }
   }
   printf( "\n" );
}


#ifdef __STANDALONE__


int main( int argc, char const * const argv[] )
{
   printf( "Hello %s\n", argv[0] );

   rollingMedian_t rm( 5 );
   while( 1 )
   {
      char inBuf[80];
      if( fgets( inBuf, sizeof(inBuf), stdin ) )
      {
         unsigned short nextVal ;
         if( 1 == sscanf( inBuf, "%hu", &nextVal ) )
         {
            printf( "add: %u\n", nextVal );
            rm.feed( nextVal );
         }
         rm.dump();
      }
      else
         break;
   }
   return 0 ;
}

#endif
