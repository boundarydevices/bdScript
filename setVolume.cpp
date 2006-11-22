#include <stdio.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>


int main( int argc, char const * const argv[] )
{
   if( 3 == argc ){
      int dspFd = open( argv[1], O_WRONLY );
      if( 0 < dspFd ){
         unsigned long volume = strtoul(argv[2],0,0);
         volume = (volume << 8) | volume ;
         int rval = ioctl( dspFd, SOUND_MIXER_WRITE_VOLUME, &volume);
         if( 0 == rval ){
            printf( "volume changed\n" );
         }
         else
            perror(argv[2]);

         close( dspFd );
      }
      else
         perror( argv[1] );
   }
   else
      printf( "Usage: setVolume device volume(0-99)\n" );
   return 0 ;
}

