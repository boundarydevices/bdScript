#include <unistd.h>
#include <linux/soundcard.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "hexDump.h"

static void normalize( short int *samples,
                       unsigned   numSamples )
{
   signed short min = 0x7fff ;
   signed short max = 0x8000 ;
   signed short *next = samples ;
   for( unsigned i = 0 ; i < numSamples ; i++ )
   {
      signed short s = *next++ ;
      if( s > max )
         max = s ;
      if( s < min )
         min = s ;
   }
   min = 0-min ;
   max = max > min ? max : min ;
   
   printf( "max sample %d\n", max );
   
   //
   // fixed point 16:16
   //
   unsigned long const ratio = ( 0x70000000UL / max );
   printf( "ratio %lx\n", ratio );

   next = samples ;
   for( unsigned i = 0 ; i < numSamples ; i++ )
   {
      signed long x16 = ratio * *next ;
      signed short s = (signed short)( x16 >> 16 );
      *next++ = s ;
   }
}

int main( int argc, char const *const argv[] )
{
   int fd = open( "/dev/dsp", O_RDONLY );
   if( 0 <= fd )
   {
      printf( "opened /dev/dsp\n" );
      if( 0 == ioctl( fd, SNDCTL_DSP_SYNC, 0 ) ) 
      {
         int const format = AFMT_S16_LE ;
         if( 0 == ioctl( fd, SNDCTL_DSP_SETFMT, &format) ) 
         {
            int const channels = 1 ;
            if( 0 != ioctl( fd, SNDCTL_DSP_CHANNELS, &channels ) )
               fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS)\n" );

            int speed = 44100 ;
            while( 0 < speed )
            {
               if( 0 == ioctl( fd, SNDCTL_DSP_SPEED, &speed ) )
               {
                  printf( "set speed to %u\n", speed );
                  break;
               }
               else
                  fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", speed );
               speed /= 2 ;
            }
            
            int recordLevel = 0 ;
            if( 0 == ioctl( fd, MIXER_READ( SOUND_MIXER_MIC ), &recordLevel ) )
               printf( "record level was %d\n", recordLevel );
            else
               perror( "get record level" );

            recordLevel = 10 ;
            if( 0 == ioctl( fd, MIXER_WRITE( SOUND_MIXER_MIC ), &recordLevel ) )
               printf( "record level is now %d\n", recordLevel );
            else
               perror( "set record level" );

            if( 0 == ioctl( fd, MIXER_READ( SOUND_MIXER_MIC ), &recordLevel ) )
               printf( "record level was %d\n", recordLevel );
            else
               perror( "get record level" );

            int recSrc ;
            if( 0 == ioctl( fd, MIXER_READ( SOUND_MIXER_RECSRC ), &recSrc ) )
               printf( "recSrc %x\n", recSrc );
            else
               perror( "get record srcs" );
            
            int recMask ;
            if( 0 == ioctl( fd, MIXER_READ( SOUND_MIXER_RECMASK ), &recMask ) )
               printf( "recMask %x\n", recMask );
            else
               perror( "get record mask" );

            unsigned long numSamples = speed * 2 ;
            unsigned short * const samples = new unsigned short [ numSamples ];
            unsigned short *nextSample = samples ;
            unsigned long bytesLeft = numSamples * sizeof( samples[0] );
            while( 0 < bytesLeft )
            {
               int numRead = read( fd, nextSample, bytesLeft );
               if( 0 < numRead )
               {
                  printf( "read %d bytes\n", numRead );
                  bytesLeft -= numRead ;
               }
               else
               {
                  perror( "read" );
                  break;
               }
            }
            close( fd );
            
            hexDumper_t dumpData( samples, 128 );
            while( dumpData.nextLine() )
               printf( "%s\n", dumpData.getLine() );

            normalize( (short *)samples, numSamples );

            fd = open( "/dev/dsp", O_WRONLY );
            if( 0 <= fd )
            {
               int const channels = 1 ;
               if( 0 != ioctl( fd, SNDCTL_DSP_CHANNELS, &channels ) )
                  fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS)\n" );

               if( 0 == ioctl( fd, SNDCTL_DSP_SPEED, &speed ) )
               {
                  printf( "set speed to %u\n", speed );
               }
               else
                  fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", speed );
               
               int vol = 0x4646 ;
               if( 2 <= argc )
               {
                  int reqVol = atoi( argv[1] );
                  if( ( 0 <= reqVol ) && ( 100 >= reqVol ) )
                  {
                     printf( "volume is %d\n", reqVol );
                     vol = ( reqVol << 8 ) | ( reqVol );
                     printf( "vol is 0x%04x\n", vol );
                  }
               }

               if( 0 > ioctl( fd, SOUND_MIXER_WRITE_VOLUME, &vol)) 
                  perror( "setVolume" );

               nextSample = samples ;
               bytesLeft = numSamples * sizeof( samples[0] );
               while( 0 < bytesLeft )
               {
                  int numWritten = write( fd, nextSample, bytesLeft );
                  if( 0 < numWritten )
                  {
                     printf( "wrote %d bytes\n", numWritten );
                     bytesLeft -= numWritten ;
                  }
                  else
                  {
                     perror( "write" );
                     break;
                  }
               }

               sleep( 2 );
               ioctl( fd, SNDCTL_DSP_SYNC, 0 );

            }
            else
               perror( "open2" );
         }
         else
            perror( "DSP_SETFMT" );
      }
      else
         perror( "DSP_SYNC" );

      close( fd );
   }
   else
      perror( "/dev/dsp" );

   return 0 ;
}
