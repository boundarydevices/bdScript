/*
 * Module ultodd.cpp
 *
 * This module defines the methods of the
 * ultodd_t class as declared in ultodd.
 *
 *
 * Change History : 
 *
 * $Log: ultodd.cpp,v $
 * Revision 1.1  2002-11-11 04:30:46  ericn
 * -moved from boundary1
 *
 * Revision 1.1  2002/09/10 14:30:44  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "ultodd.h"
#include "ultoa.h"

ultodd_t :: ultodd_t( unsigned long ip )
{
   unsigned char const *bytes = (unsigned char const *)&ip ;
   char *nextOut = dd_ ;
   for( unsigned i = 0 ; i < 4 ; i++ )
   {
      ultoa_t decimal( *bytes++, 1, '0' );
      char const *s = decimal.getValue();
      while( *s )
         *nextOut++ = *s++ ;
      *nextOut++ = '.' ;
   }

   nextOut[-1] = '\0' ; // terminate
}

