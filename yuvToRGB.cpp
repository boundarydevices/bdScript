#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

int main( int argc, char const * const argv[] )
{
   if( 4 == argc ){
      unsigned long y = strtoul( argv[1], 0, 0 );
      unsigned long u = strtoul( argv[2], 0, 0 );
      unsigned long v = strtoul( argv[3], 0, 0 );

      long C = y - 16 ;
      long D = u - 128 ;
      long E = v - 128 ;

      unsigned char r = (unsigned short)(298*C         + 409*E + 128) >> 8 ;
      unsigned char g = (unsigned short)(298*C + 100*D - 208*E + 128) >> 8 ;
      unsigned char b = (unsigned short)(298*C + 516*D         + 128) >> 8 ;

      printf( "yuv(%lu,%lu,%lu) == rgb(%lu,%lu,%lu)\n", y, u, v, r, g, b );
      printf( " == 0x%04x\n", (r&0xf8)<<(11-3) | (g&0xfc)<<(5-2) | (b&0xf8)>>3 );
   }
   else
      fprintf( stderr, "Usage: %s y(0-65535) u(0-65535) v(0-65535)\n", argv[0] );

   return 0;
}
