#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char const * const argv[] )
{
   if( 1 < argc ){
      for( int i = 1 ; i < argc ; i++ ){
          FILE *f = fopen( argv[i], "r" );

          if( !f ){
              perror( argv[i] );
          }
      }

      while(1){
         sleep(10); printf( "zzzz\n" );
      }
   }
   else
      fprintf( stderr, "Usage: %s file [file...]\n", argv[0] );

   return 0 ;
}
