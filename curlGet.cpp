/*
 * Module curlGet.cpp
 *
 * This module defines the curlGet() routine,
 * as declared in curlGet.h
 *
 *
 * Change History : 
 *
 * $Log: curlGet.cpp,v $
 * Revision 1.1  2002-10-06 16:52:32  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "curlGet.h"
#include "curlCache.h"
#include <string.h>
#include <stdio.h>

bool curlGet( char const *url,
              char const *saveAs )
{
   curlCache_t &cache = getCurlCache();

   curlFile_t f( cache.get( url ) );

   bool worked = false ;

   if( f.isOpen() )
   {
      if( 0 == saveAs )
      {
         char const *tail = url + strlen( url );
         while( tail > url )
         {
            char const c = *(--tail);
            if( ( '\\' == c ) || ( '/' == c ) )
            {
               tail++ ;
               break;
            }
         }
         saveAs = tail ;
      }
      FILE *fOut = fopen( saveAs, "wb" );
      if( fOut )
      {
         worked = ( f.getSize() == fwrite( f.getData(), 1, f.getSize(), fOut ) );
         worked = ( 0 == fclose( fOut ) ) && worked ;
      } // opened target file
   }
   
   return worked ;
}


#ifdef STANDALONE
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

int main( int argc, char const * const argv[] )
{
   int returnVal ;

   if( ( 2 == argc ) || ( 3 == argc ) )
   {
      char const *target = ( 3 == argc ) ? argv[2] : 0 ;
      if( curlGet( argv[1], target ) )
      {
         char path[256];
         printf( "%s -> %s\n", argv[1], ( 0 == target ) ? getcwd( path, sizeof( path )) : target );
         returnVal = 0 ;
      }
      else
      {
         fprintf( stderr, "Error getting %s\n", argv[1] );
         returnVal = ENOENT ;
      }
   }
   else
   {   
      fprintf( stderr, "Usage : curlGet url [saveFileName]\n" );
      returnVal = EINVAL ;
   }
   
   return returnVal ;
}

#endif
