/*
 * Program starImage.cpp
 *
 * This is a simple test program to send graphics
 * commands to the Star Micronics TUP900 printer
 *
 *
 * Change History : 
 *
 * $Log: starImage.cpp,v $
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
#include <assert.h>
#include "imgFile.h"
#include "bdGraph/Scale16.h"
#include "dither.h"

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

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      int const fd = open( "/dev/usb/lp0", O_WRONLY );
      if( 0 <= fd )
      {
         printf( "printer port opened\n" );
         SENDCOMMAND( initPrinter );
         SENDCOMMAND( rasterMode );
         SENDCOMMAND( letterQuality );
   
         SENDCOMMAND( leftMargin );
         SENDCOMMAND( rightMargin );
         
   
         for( int arg = 1 ; arg < argc ; arg++ )
         {
            printf( "%s\n", argv[arg] );
            image_t img ;
            if( imageFromFile( argv[arg], img ) )
            {
               unsigned newW = img.width_ ;
               unsigned newH = img.height_ ;
               printf( "%u x %u pixels\n", img.width_, img.height_ );
               unsigned short *pixData = (unsigned short *)img.pixData_ ;
               if( img.width_ < (PAGEWIDTH/2) )
               {
                  unsigned mult = ( PAGEWIDTH / img.width_ );
                  newW = img.width_ * mult ;
                  newH = img.height_ * mult ;
                  pixData = new unsigned short[newW*newH*sizeof(pixData[0])];
                  Scale16::scale( pixData, newW, newH, 
                                  (unsigned short const *)img.pixData_, img.width_, img.height_, 0, 0, img.width_, img.height_ );

               }
               else
               {
               }

               printf( "scale to %u x %u\n", newW, newH );
               // set page height
               pprintf( fd, "\x01b*rP%d", newH );
               write( fd, "\x00", 1 );
               SENDCOMMAND( clearImage );

               dither_t dither( pixData, newW, newH );
               unsigned const rowBytes = (newW+7)/8 ;
               unsigned const imageBytes = newH*(rowBytes+3);
               unsigned char * const rasterData = new unsigned char [imageBytes];
               unsigned char *nextOut = rasterData ;
               unsigned char const n = rowBytes / 256;
               unsigned char const m = rowBytes & 255 ;

               for( unsigned row = 0 ; row < newH ; row++ )
               {
                  *nextOut++ = 'b' ;
                  *nextOut++ = m ;
                  *nextOut++ = n ;
         
                  unsigned char mask = '\x80' ;
                  unsigned char out = 0 ;
                  for( unsigned col = 0 ; col < newW ; col++ )
                  {
                     if( dither.isBlack( col, row ) )
                        out |= mask ;
                     mask >>= 1 ;
                     if( 0 == mask )
                     {
                        mask = '\x80' ;
                        *nextOut++ = out ;
                        out = 0 ;
                     }
                  }
                  
                  if( '\x80' != mask )
                     *nextOut++ = out ;
               }
               assert( nextOut == rasterData + imageBytes );

               write( fd, rasterData, imageBytes );

               SENDCOMMAND( formFeed );

               delete [] rasterData ;

               if( pixData != (unsigned short *)img.pixData_ )
                  delete [] pixData ;
            }
            else
               perror( argv[arg] );
         }
         
/*
         unsigned const imageBytes = PAGEHEIGHT*(WIDTHBYTES+3);
         unsigned char * const rasterData = new unsigned char [imageBytes];
         unsigned char *nextOut = rasterData ;
         unsigned char const n = WIDTHBYTES / 256;
         unsigned char const m = WIDTHBYTES & 255 ;
   
         for( unsigned i = 0 ; i < PAGEHEIGHT ; i++ )
         {
            *nextOut++ = 'b' ;
            *nextOut++ = m ;
            *nextOut++ = n ;
   
            memset( nextOut, i, WIDTHBYTES );
            nextOut += WIDTHBYTES ;
         }
         assert( nextOut == rasterData + imageBytes );
   //      write( fd, rasterData, imageBytes );
   
*/   
         fsync( fd );
         close( fd );
         printf( "printer port closed\n" );
      }
      else
         perror( "/dev/usb/lp0" );
   }
   else
      fprintf( stderr, "Usage: %s infile.jpg [inFile2.png]\n", argv[0] );
   
   return 0 ;
}

