/*
 * Program tsTest.cpp
 *
 * This module defines the tsTest() routine, which simply
 * dumps the input
 *
 *
 * Change History : 
 *
 * $Log: tsTest.cpp,v $
 * Revision 1.3  2002-12-26 15:13:51  ericn
 * -fixed overrun problem
 *
 * Revision 1.2  2002/12/23 05:10:45  ericn
 * -bunch of changes to measure touch screen performance
 *
 * Revision 1.1  2002/10/25 02:55:01  ericn
 * -initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <linux/input.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <tslib.h>

// #define INPUTAPI

static void diffTime( timeval const &now, 
                      timeval const &start, 
                      timeval       &diff )
{
   if( now.tv_usec >= start.tv_usec )
   {
      diff.tv_usec = now.tv_usec - start.tv_usec ;
      diff.tv_sec  = now.tv_sec - start.tv_sec ;
   }
   else
   {
      diff.tv_usec = 1000000 + now.tv_usec - start.tv_usec ;
      diff.tv_sec = now.tv_sec - start.tv_sec - 1 ;
   }
}

static unsigned const maxSamples = 512 ;

static void showSamples( timeval const &now, timeval const &start, 
                         ts_sample const samples[], 
                         unsigned long numSamples )
{
   if( numSamples > maxSamples )
      numSamples = maxSamples ;
   
   timeval diff ;
   diffTime( now, start, diff );
   printf("%ld.%06ld: %lu\n", diff.tv_sec, diff.tv_usec, numSamples );
   for( unsigned long i = 0 ; i < numSamples ; i++ )
   {
      printf( "%ld.%06ld,%ld,%ld,%ld\n", 
              samples[i].tv.tv_sec, samples[i].tv.tv_usec, 
              samples[i].x, samples[i].y, samples[i].pressure );
   }
}

int main( int argc, char const * const argv[] )
{
   char const *deviceName = "/dev/touchscreen/ucb1x00" ;
   if( 2 == argc )
      deviceName = argv[1];
   
   ts_sample *const sampleBuf = new ts_sample [ maxSamples ];

#ifdef INPUTAPI
   int fd = open( deviceName, O_RDONLY );
   if( 0 < fd )
   {
      printf( "%s opened\n", deviceName );
      bool wasDown = false ;
      unsigned long numSamples = 0 ;
      int next_x, next_y;
      timeval startTime ;
      startTime.tv_sec = 0 ;
      startTime.tv_usec = 0 ;

      while( 1 )
      {
         struct input_event events[10];
         int const numRead = read( fd, events, sizeof( events ) );
         if( sizeof( input_event ) <= numRead )
         {
            int const numEvents = numRead / sizeof( input_event );
            for( int ev = 0 ; ev < numEvents ; ev++ )
            {
               input_event const &event = events[ev];
               if (event.type == EV_ABS) switch (event.code) {
                  case ABS_X:
                     next_x = event.value ;
                     break;
                  case ABS_Y:
                     next_y = event.value ;
                     break;
                  case ABS_PRESSURE:
                     {
                        bool isDown = ( 0 != event.value );
                        if( isDown )
                        {
                           if( !wasDown )
                           {
                              startTime = event.time ;
                              numSamples = 1 ;
                           }
                           else
                              numSamples++ ;
                           wasDown = true ;
                           if( numSamples <= maxSamples )
                           {
                              ts_sample &tsSamp = sampleBuf[numSamples-1];
                              tsSamp.pressure = event.value ;
                              tsSamp.x        = next_x ;
                              tsSamp.y        = next_y ;
                              tsSamp.tv       = event.time ;    
                           }
                        }
                        else if( wasDown )
                        {
                           showSamples( event.time, startTime, sampleBuf, numSamples );
                           numSamples = 0 ;
                           wasDown = false ;
                        }
                        break;
                     }
               }
            }
         }
         else
         {
            fprintf( stderr, "Short read %d : %m\n", numRead );
            break;
         }
      }
      close( fd );
   }
   else
      perror( deviceName );
#else
   struct tsdev *ts = ts_open( deviceName, 0);
   if( ts )
   {
      bool wasDown = false ;
      unsigned long numSamples = 0 ;
      timeval startTime ;
      startTime.tv_sec = 0 ;
      startTime.tv_usec = 0 ;
      
      while (1) {
         struct ts_sample samples[1];
         int ret ;
         if( 0 < ( ret = ts_read_raw( ts, samples, 1) ) )
         {
            unsigned numRead = (unsigned)ret ;
            for( unsigned i = 0 ; i < numRead ; i++ )
            {
               ts_sample const &samp = samples[i];
               bool isDown = ( 0 != samp.pressure );
               if( isDown )
               {
   //               printf( "d\n" );
                  if( !wasDown )
                  {
                     startTime = samp.tv ;
                     numSamples = 1 ;
                  }
                  else
                  {
                     numSamples++ ;
                  }
                  
                  if( numSamples <= maxSamples )
                  {
                     sampleBuf[ numSamples-1 ] = samp ;
                  }

                  wasDown = true ;
               }
               else if( wasDown && ( 2000 > samp.y ) )
               {
   //               printf( "u\n" );
                  showSamples( samp.tv, startTime, sampleBuf, numSamples );
                  numSamples = 0 ;
                  wasDown = false ;
               }
            }
         }
         else if( 0 > ret )
         {
            perror( "ts_read_raw" );
            break;
         }
      }
   }
   else
      perror( deviceName );
#endif
   
   return 0 ;
}
