/*
 * dlopen.cpp
 * 
 * Test program to check for dependency problems
 * in dynamic libraries.
 *
 */
 
#include <stdio.h>
#include <dlfcn.h>

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         void *handle = dlopen( argv[arg], RTLD_NOW );
         if( handle )
         {
            printf( "%s opened\n", argv[arg] );
            if( 0 == dlclose( handle ) )
               printf( "closed\n" );
            else
               fprintf( stderr, "close error %s\n", dlerror() );
         }
         else
            fprintf( stderr, "Error loading %s:%s\n", argv[arg], dlerror() );
      }
   }
   else
      fprintf( stderr, "Usage : dlopen libname [...]\n" );

   return 0 ;
}
