/*
 * Program dictionary.cpp
 *
 * Simple test program for the dictionary_t template.
 *
 *
 * Change History : 
 *
 * $Log: dictionary.cpp,v $
 * Revision 1.1  2006-06-14 13:54:07  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "dictionary.h"

#include <stdio.h>
#include <string>
#include <string.h>

int main( int argc, char const * const argv[] )
{
   dictionary_t<std::string> dict ;
   printf( "Hello, %s\n", argv[0] );
   
   char inBuf[256];
   while( fgets( inBuf, sizeof(inBuf), stdin ) )
   {
      unsigned idx = ( dict += std::string(inBuf,strlen(inBuf)-1) );
      printf( "%u\n", idx );
   }
   
   printf( "%u entries\n", dict.size() );
   for( unsigned i = 0 ; i < dict.size(); i++ )
   {
      printf( "[%u] == %s\n", i, dict[i].c_str() );
   }
   
   return 0 ;
}
