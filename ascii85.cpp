/*
 * Module ascii85.cpp
 *
 * This module defines the methods of the ascii85_t class
 * as declared in ascii85.h
 *
 *
 * Change History : 
 *
 * $Log: ascii85.cpp,v $
 * Revision 1.1  2006-10-29 21:59:10  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "ascii85.h"
#include <netinet/in.h>
#include <string.h>
#include <assert.h>
// #define DEBUGPRINT
#include "debugPrint.h"

ascii85_t::ascii85_t( 
   ascii85_output_t outHandler,
   void            *outParam )
   : handler_( outHandler )
   , hParam_( outParam )
   , numLeft_( 0 )
{
}

static unsigned long const power85[5] = { 
   1L, 
   85L, 
   85L*85, 
   85L*85*85, 
   85L*85*85*85
};

bool ascii85_t::convert
   ( char const *inData,
     unsigned    inLen )
{
debugPrint( "ascii85::convert: %p/%u/%u\n", inData, inLen, numLeft_ );

   while( 4 <= ( inLen + numLeft_ ) ){
      unsigned num = 4-numLeft_ ;
//debugPrint( "ascii85::convert: %u\n", inLen );
      memcpy( leftover_+numLeft_, inData, num );
if( 2246 == inLen )
   debugPrint( "got here\n" );
      inLen  -= num ;
      inData += num ;
      unsigned long lval = ntohl( *(unsigned long *)leftover_ );
      if( 0 == lval ){
         if( !handler_('z', hParam_) ){
debugPrint( "ascii85::~convert: %p/%u\n", inData, inLen );
            return false ;
         }
      }
      else {
         for( int i = 4 ; 0 <= i ; --i ){
            unsigned long const p85 = power85[i];
assert( 0 != p85 );
            unsigned long const v = lval / p85 ;
            if( !handler_((char)(v+'!'), hParam_) )
               return false ;
            lval -= v*p85 ;
         }
      }
      numLeft_ = 0 ;
   }

debugPrint( "ascii85::~convert2: %p/%u\n", inData, inLen );
   if( 0 < inLen ){
      assert( numLeft_ + sizeof(leftover_) >= inLen );
      memcpy( leftover_+numLeft_, inData, inLen );
      numLeft_ += inLen ;
   }

debugPrint( "ascii85::~convert3: %p/%u\n", inData, inLen );
   return true ;
}

bool ascii85_t::flush( void )
{
   if( 0 < numLeft_ ){
      memset( leftover_+numLeft_, sizeof(leftover_)-numLeft_, 0 );
      unsigned long lval = ntohl( *(unsigned long *)leftover_ );
      if( 0 == lval ){
         if( !handler_('z', hParam_) )
            return false ;
      } else {
         int const stop = 4-numLeft_ ; // output numLeft_+1 bytes
         for( int i = 4 ; stop <= i ; --i ){
            unsigned long const p85 = power85[i];
            unsigned long const v = lval / p85 ;
            if( !handler_((char)(v+'!'), hParam_) )
               return false ;
            lval -= v*p85 ;
         }
      }
   }
   numLeft_ = 0 ;
   return true ;
}


#ifdef MODULETEST
#include <stdio.h>

static int outPos = 0 ;

static bool outHandler( char  outchar, void *opaque )
{
   FILE *fOut = (FILE *)opaque ;
   fwrite( &outchar, 1, 1, fOut );
   if( ( ++outPos > 63 )
       ||
       ( ( 1 == outPos ) && ( '%' == outchar ) ) )
   {
      fputc( '\n', fOut );
      outPos = 0 ;
   }

   return true ;
}


int main( int argc, char const * const argv[] )
{
   if( 2 <= argc ){
      FILE *fIn = fopen( argv[1], "rb" );
      if( fIn ){
         char inBuf[256];
         int const readLengths[4] = {
            1, 4, 7, sizeof(inBuf)
         };

         ascii85_t a85( outHandler, stdout );

         unsigned iterations = 0 ;
         int numRead ;
         while( 0 < ( numRead = fread( inBuf, 1, readLengths[++iterations&3], fIn ) ) )
         {
            if( !a85.convert( inBuf, numRead ) ){
               fprintf( stderr, "conversion error\n" );
               return -1 ;
            }
         }
   
         if( a85.flush() ){
            printf( "\nconversion complete\n" );
         } else {
            fprintf( stderr, "Flush error\n" );
         }

         fclose( fIn );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s inFile\n", argv[0] );

   return 0 ;
}

#endif
