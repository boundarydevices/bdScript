/*
 * Program starGraph.cpp
 *
 * This is a simple test program to send graphics
 * commands to the Star Micronics TUP900 printer
 *
 *
 * Change History : 
 *
 * $Log: starGraph.cpp,v $
 * Revision 1.1  2004-05-02 14:07:22  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/lp.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

static char const rasterMode[] = {
   "\x1b*rR"            // initialize raster mode (to defaults)
   "\x1b*rA"            // enter raster mode
};

static char const initPrinter[] = {
   "\x1b@"
};

static char const letterQuality[] = {
   "\x1b*rQ2\x00"
};

static char const clearImage[] = {
   "\x1b*rC" 
};

static char const leftMargin[] = {
   "\x1b*rml1\x00"
};

static char const rightMargin[] = {
   "\x1b*rmr1\x00"
};

static char const topMargin[] = {
   "\x1b*rT0\x00"
};

static char const formFeed[] = {
   "\x1b\x0C\x00" 
};

static int writeCount( int fd, char const *data, int length )
{
   int const numWritten = write( fd, data, length );
   printf( "wrote %d bytes\n", numWritten );
   while( length-- )
      printf( "%02x ", *data++ );
   printf( "\n" );
   return numWritten ;
}

static int pprintf( int fd, char const *fmt, ... )
{
   char buf[512];
   va_list args;

   va_start( args, fmt );
   
   int const len = vsprintf( buf, fmt, args );
   int const numWritten = writeCount( fd, buf, len );
   
   va_end( args );

   return numWritten ;
}

static int writeBits( int fd, unsigned char const data[], unsigned bytes )
{
   int const n = (int) bytes / 256;
   int const m = (bytes > 0) ? (bytes&255) : 1 ;
   char header[4];
   sprintf( header, "b%c%c\n", (char)m, (char)n );
   int numWritten = write( fd, header, 3 );

   numWritten += write( fd, data, bytes );

//   printf( "%d bytes of raster data\n", numWritten );

   return numWritten ;

}

#define SENDCOMMAND( data ) writeCount( fd, data, sizeof(data)-1 );

#define DPI          203
#define PAGEHEIGHT   (DPI*2)
#define PAGEWIDTH    832
#define WIDTHBYTES   (PAGEWIDTH/8)

int main( void )
{
   int const fd = open( "/dev/usb/lp0", O_WRONLY );
   if( 0 <= fd )
   {
      printf( "printer port opened\n" );
      SENDCOMMAND( initPrinter );
      SENDCOMMAND( rasterMode );
      SENDCOMMAND( letterQuality );

      // set page height
      pprintf( fd, "\x01b*rP%d", PAGEHEIGHT );
      write( fd, "\x00", 1 );

      SENDCOMMAND( clearImage );

      SENDCOMMAND( leftMargin );
      SENDCOMMAND( rightMargin );
      
      unsigned char rasterData[WIDTHBYTES];

      for( unsigned i = 0 ; i < PAGEHEIGHT ; i++ )
      {
         memset( rasterData, i, sizeof( rasterData ) );
         writeBits( fd, rasterData, sizeof( rasterData ) );
      }

      SENDCOMMAND( formFeed );

      fsync( fd );
      close( fd );
      printf( "printer port closed\n" );
   }
   else
      perror( "/dev/usb/lp0" );
   
   return 0 ;
}

