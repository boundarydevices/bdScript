/*
 * Program markSpace.cpp
 *
 * This program is designed to test mark/space parity
 * under Linux. 
 * 
 * It will toggle between mark and space parity, printing
 * alternately "Mark" and "Space", waiting 1 second between
 * each (long enough to NOW FIFOs).
 *
 *
 * Change History : 
 *
 * $Log: markSpace.cpp,v $
 * Revision 1.1  2006-05-13 17:29:59  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define CMSPAR   010000000000

static bool volatile doExit = false ;


static void ctrlcHandler( int )
{
   doExit = true ;
}

static void setMark( int fd, struct termios const &origTio )
{
   struct termios tio = origTio ;
   tio.c_cflag |= PARENB | CMSPAR | PARODD ;
   int const result = tcsetattr( fd, TCSADRAIN, &tio );
   if( 0 != result )
      perror( "tcsetattr:MARK" );
}

static void setSpace( int fd, struct termios const &origTio )
{
   struct termios tio = origTio ;
   tio.c_cflag |= PARENB | CMSPAR;
   tio.c_cflag &= ~PARODD;
   int const result = tcsetattr( fd, TCSADRAIN, &tio );
   if( 0 != result )
      perror( "tcsetattr:SPACE" );
}

typedef void (*setLine_t)( int fd, struct termios const &origTio );

static setLine_t setLine[2] = {
   setMark,
   setSpace
};

static char const *msgs[2] = {
   "MARK.",
   "SPACE"
};

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      char const * const deviceName = argv[1];
      int fdSerial = open( deviceName, O_RDWR );
      if( 0 <= fdSerial )
      {
         printf( "device %s opened\n", deviceName );
         signal(SIGINT,ctrlcHandler);

         struct termios origTio ;
         if( 0 == tcgetattr( fdSerial, &origTio ) )
         {
            unsigned i = 0 ;
            while( !doExit )
            {
               setLine[i&1]( fdSerial, origTio );
               write( fdSerial, msgs[i&1], 5 );
               write( fdSerial, "\r\n", 2 );
               i++ ;
            }
            
            printf( "restoring %s\n", deviceName );
            tcsetattr( fdSerial, TCSADRAIN, &origTio );
         }
         else
            perror( "tcgetattr" );
         
         close( fdSerial );
      }
      else
         perror( deviceName );
   }
   else
      fprintf( stderr, "Usage: markSpace deviceName\n" );
   return 0 ;
}
