/*
 * Module md5.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: md5.cpp,v $
 * Revision 1.1  2003-09-06 18:57:43  ericn
 * -initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "md5.h"
#include "openssl/md5.h"

void getMD5( void const   *data,
             unsigned long bytes,
             md5_t        &result )
{
   MD5_CTX ctx ;
   MD5_Init( &ctx );
   MD5_Update( &ctx, data, bytes );
   MD5_Final( result.md5bytes_, &ctx );
}


#ifdef STANDALONE
#include <stdio.h>
#include "memFile.h"

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         memFile_t fIn( argv[arg] );
         if( fIn.worked() )
         {
            md5_t md5 ;
            getMD5( fIn.getData(), fIn.getLength(), md5 );
            for( unsigned dig = 0 ; dig < sizeof( md5.md5bytes_ ); dig++ )
               printf( "%02x", md5.md5bytes_[ dig ] );
            printf( "\n" );
         }
         else
            perror( argv[1] );
      }
   }
   else
      fprintf( stderr, "Usage: %s fileName [fileName...]\n", argv[0] );

   return 0 ;
}

#endif
