/*
 * Module ultoa.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: ultoa.cpp,v $
 * Revision 1.1  2002-11-11 04:30:45  ericn
 * -moved from boundary1
 *
 * Revision 1.1  2002/09/10 14:30:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "ultoa.h"


ultoa_t :: ultoa_t
   ( unsigned long value,
     signed char   minWidth,       // < 0 to left-justify
     char          padChar,
     base_e        radix )
{
   static char const charSet[] = { '0','1','2','3',
                                   '4','5','6','7',
                                   '8','9','A','B',
                                   'C','D','E','F' };

   char reversed[ sizeof( charValue_ ) ];
   char *lastRev = reversed ;

   //
   // build the string reversed
   //
   if( 0 == value )
      *lastRev++ = '0' ;
   else
   {
      do {
         unsigned char low = ( unsigned char )( value % radix );
         *lastRev++ = charSet[ low ];
         value /= radix ;
      } while( 0 != value );
   }

   unsigned const lengthBeforePad = lastRev - reversed ;

   if( 0 >= minWidth )
   {
      if( 0 == minWidth )
         minWidth = lengthBeforePad ;
      else
         minWidth = 0 - minWidth ;

      // first reverse the string, then keep padding until proper length
      //
      char *nextFwd = charValue_ ;
      while( lastRev > reversed )
      {
         lastRev-- ;
         *nextFwd++ = *lastRev ;
      }

      if( (unsigned) minWidth < lengthBeforePad )
         minWidth = lengthBeforePad ;

      for( unsigned length = nextFwd - charValue_ ; length < ( unsigned )minWidth ; length++ )
         *nextFwd++ = padChar ;

      *nextFwd = '\0' ;

   } // left justify
   else
   {
      if( 0 == minWidth )
         minWidth = 1 ;

      // first reverse the string into proper position,
      //    then keep padding until we hit the start of string
      //
      if( lengthBeforePad > ( unsigned )minWidth )
         minWidth = lengthBeforePad ;

      char *nextFwd = charValue_ + minWidth ;

      *nextFwd-- = '\0' ;

      for( char *pRev = reversed ; pRev < lastRev ; )
      {
         *nextFwd-- = *pRev++ ;
      }

      while( nextFwd >= charValue_ )
         *nextFwd-- = padChar ;
   } // right justify

} // ultoa_t :: constructor


