#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

int main( int argc, char const * const argv[] )
{
   if( 4 == argc ){
      char const *const devName = argv[1];
      int fdDev = open( devName, O_RDONLY );
      unsigned long const state = strtoul(argv[2],0,0) & 1;

      if( 0 <= fdDev ){
         char inData[80];
         int numRead ;
         while( 0 < (numRead = read( fdDev, inData, sizeof(inData))) )
         {
            for( int i = 0 ; i < numRead ; i++ ){
               if( state == (inData[i]&1) ){
                  system(argv[3]);
               }
            }
         }
         close( fdDev );
      }
      else
         perror( devName );
   }
   else
      fprintf( stderr, "Usage: %s /dev/gp 1|0 command\n", argv[0] );

   return 0 ;
}
