/*
 * Module ddtoul.cpp
 *
 * This module defines the methods of the ddtoul_t 
 * class as declared in ddtoul.h
 *
 *
 * Change History : 
 *
 * $Log: ddtoul.cpp,v $
 * Revision 1.1  2002-11-11 04:30:45  ericn
 * -moved from boundary1
 *
 * Revision 1.1  2002/09/10 14:30:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "ddtoul.h"
#include "macros.h"

ddtoul_t :: ddtoul_t( char const *dotted )
   : netOrder_( 0 ),
     hostOrder_( 0 ),
     worked_( true )
{
   //
   // build in network order, then convert
   //
   unsigned char  byteId = 0 ;
   unsigned char *netBytes = (unsigned char *)&netOrder_ ;

   for( ; ( byteId < 4 ) && ( worked_ ) ; byteId++, netBytes++ )
   {
      unsigned long byteValue = 0 ;

      while( worked_ )
      {
         char const dig = *dotted++ ;
         if( ( '0' <= dig ) && ( '9' >= dig ) )
         {
            byteValue *= 10 ;
            byteValue += ( dig - '0' );
         }
         else if( '\0' == dig )
         {
            worked_ = ( 3 == byteId );
            break;
         }
         else if( '.' == dig )
         {
            worked_ = ( 3 > byteId );
            break;
         }
         else
            worked_ = false ;
      } // process segment

      if( worked_ )
      {
         worked_ = ( byteValue < 256 );
         if( worked_ )
            *netBytes = (unsigned char)byteValue ;
      }

   } // for each byte

   if( worked_ )
   {
      hostOrder_ = networkToHost( netOrder_ );
   }
   else
   {
      netOrder_ = 0 ;
   }
}
