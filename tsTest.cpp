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
 * Revision 1.1  2002-10-25 02:55:01  ericn
 * -initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <linux/input.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main( int argc, char const * const argv[] )
{
   int fd = open( argv[1], O_RDONLY );
   if( 0 < fd )
   {
      printf( "%s opened\n", argv[1] );
      while( 1 )
      {
         struct input_event event ;
         int const numRead = read( fd, &event, sizeof( event ) );
         if( sizeof( event ) == numRead )
         {
            printf( "read something\n" );
            printf( "time  == %ld.%06ld\n", event.time.tv_sec, event.time.tv_usec );
            printf( "type  == %d\n", event.type  );
            printf( "code  == %d\n", event.code  );
            printf( "value == %d\n", event.value );
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
      perror( argv[1] );
   return 0 ;
}
