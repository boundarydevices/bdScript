/*
 * Module dumpCPP.cpp
 *
 * This module defines the methods of the dumpCPP_t
 * class as declared in dumpCPP.h
 *
 *
 * Change History : 
 *
 * $Log: dumpCPP.cpp,v $
 * Revision 1.1  2004-04-07 12:45:21  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "dumpCPP.h"
#include <string.h>

static const char hexChars[] = { 
   '0', '1', '2', '3',
   '4', '5', '6', '7',
   '8', '9', 'A', 'B',
   'C', 'D', 'E', 'F' 
};

static char const boilerPlate[] = {
   "                                                                           // "
};

static unsigned const bpSize = sizeof( boilerPlate );

static char *byteOut( char *nextOut, unsigned char byte )
{
   memcpy( nextOut, "\'0x", 3 );
   nextOut += 3 ;
   *nextOut++ = hexChars[ byte >> 4 ];
   *nextOut++ = hexChars[ byte & 0x0f ];
   memcpy( nextOut, "\',", 2 );
   return nextOut + 3 ;

}

bool dumpCPP_t :: nextLine( void )
{
   if( 0 < bytesLeft_ )
   {
      strcpy( lineBuf_, boilerPlate );
      char *next = lineBuf_ + 3 ;
      next = lineBuf_ + 3 ;

      unsigned const lineBytes = ( 8 < bytesLeft_ ) ? 8 : bytesLeft_ ;
      unsigned char *bytes = (unsigned char *)data_ ;

      // chars as '\xNN'

      for( unsigned i = 0 ; i < lineBytes ; i++ )
         next = byteOut( next, *bytes++ );

      if( lineBytes == bytesLeft_ )
         next[-2] = ' ' ; // get rid of trailing comma

      next = lineBuf_ + bpSize - 1 ;
      bytes = (unsigned char *)data_ ;

      for( unsigned i = 0 ; i < lineBytes ; i++ )
      {
         unsigned char c = *bytes++ ;
         if( ( ' ' <= c ) && ( '\x7f' > c ) )
            *next++ = c ;  // printable
         else
            *next++ = '.' ;
      }

      *next = 0 ;

      data_       = bytes ;
      bytesLeft_ -= lineBytes ;
      return true ;
   }
   else
      return false ;
}

#ifdef __STANDALONE__
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      struct stat st ;
      int const statResult = stat( argv[1], &st );
      if( 0 == statResult )
      {
         int fd = open( argv[1], O_RDONLY );
         if( 0 <= fd )
         {
            off_t const fileSize = lseek( fd, 0, SEEK_END );
printf( "fileSize: %u\n", fileSize );
            lseek( fd, 0, SEEK_SET );
            void *mem = mmap( 0, fileSize, PROT_READ, MAP_PRIVATE, fd, 0 );
            if( MAP_FAILED != mem )
            {
               dumpCPP_t dump( mem, fileSize );
               
               while( dump.nextLine() )
                  printf( "%s\n", dump.getLine() );

               munmap( mem, fileSize );
      
            } // mapped file
            else
               fprintf( stderr, "Error %m mapping %s\n", argv[1] );

            close( fd );
         }
         else
            fprintf( stderr, "Error %m opening %s\n", argv[1] );
      }
      else
         fprintf( stderr, "Error %m finding %s\n", argv[1] );
   }
   else
      fprintf( stderr, "Usage : hexDump fileName\n" );

   return 0 ;
}
#endif
