#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include "config.h"
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include "tickMs.h"

int main( void )
{
   printf( "hello, testYUV\n" );

   int const fdYUV = open( "/dev/yuv", O_WRONLY );
   if( 0 <= fdYUV )
   {
      int const height = 160 ;
      int const width  = 224 ;
      int const stride = width*2 ;
      unsigned const numBytes = height*width*2 ;
      unsigned char *const data = new unsigned char [ numBytes ];
      memset( data, 0, numBytes );

      char inBuf[80];
      unsigned char const mask = 0xFF ;
//      unsigned char const mask = 0x0F ;
      unsigned char nextVal = mask ;
      
      do
      {
         printf( "value == 0x%04x\n", nextVal );
         memset( data, nextVal, (3*numBytes)/4 );

         memset( data+numBytes-stride, nextVal, stride );
         write( fdYUV, data, numBytes );

         nextVal -= 0x01 ;
//         nextVal -= 0x01 ;
         nextVal &= mask ;

      } while( 1 );

      close( fdYUV );
   }
   else
      perror( "/dev/yuv" );

   return 0 ;
}
