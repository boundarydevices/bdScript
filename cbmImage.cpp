/*
 * Module cbmImage.cpp
 *
 * This module defines the methods of the cbmImage_t
 * class as declared in cbmImage.h
 *
 *
 * Change History : 
 *
 * $Log: cbmImage.cpp,v $
 * Revision 1.2  2003-06-26 08:01:33  tkisky
 * -fix last segment bugs
 *
 * Revision 1.1  2003/05/26 22:08:19  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "cbmImage.h"
#include <assert.h>
#include <stdio.h>
#include "hexDump.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/lp.h>
#include <sys/ioctl.h>
#include <string.h>

#define EVENBYTES( bits ) (((bits)+7)&~7)

static unsigned fixupWidth( unsigned width )
{
   unsigned w = EVENBYTES(width);
   if( w > cbmImage_t :: maxWidth_ )
      w = cbmImage_t :: maxWidth_ ;
   return w ;
}

static unsigned const maxN1xN2 = 1536 ;
static unsigned const headerLen = 4 ;
static unsigned const trailerLen = 3 ;

static unsigned getRowsPerSegment( unsigned width )
{
   unsigned const maxN2 = maxN1xN2/(width/8);
   return maxN2*8 ;
}

cbmImage_t :: cbmImage_t( unsigned short widthPix,
                          unsigned short heightPix )
   : specWidth_( widthPix ),
     specHeight_( heightPix ),
     actualWidth_( fixupWidth(widthPix) ),
     actualHeight_( EVENBYTES(heightPix) ),
     rowsPerSegment_( getRowsPerSegment( actualWidth_ ) ),
     rowsLastSegment_( (((actualHeight_-1) % rowsPerSegment_)+1+7)&~7 ),
     numSegments_( (actualHeight_+rowsPerSegment_-1)/rowsPerSegment_ ),
     bytesPerSegment_( (rowsPerSegment_*actualWidth_/8) + headerLen + trailerLen ),
     length_( bytesPerSegment_*numSegments_ ),
     data_( new unsigned char[ length_ ] )
{
   memset( data_, 0, length_ );
   char *nextOut = (char *)data_ ;
   unsigned char const wBytes = actualWidth_ / 8 ;

   for( unsigned seg = 0 ; seg < numSegments_ ; seg++ )
   {
      unsigned char const rowBytesInSeg = ((seg < (numSegments_-1)) ? rowsPerSegment_ : rowsLastSegment_) /8;
      nextOut += sprintf( nextOut, "\x1d*%c%c", wBytes, rowBytesInSeg );
      int bytes = actualWidth_ * rowBytesInSeg;
      printf( "seg bytes %i, wBytes:%i,hBytes:%i\n",bytes,wBytes,rowBytesInSeg );
      nextOut +=  bytes;
      nextOut += sprintf( nextOut, "\x1d/%c", 0 );
   }

   assert( nextOut == (char *)data_ + length_ );
}

cbmImage_t :: ~cbmImage_t( void )
{
   if( data_ )
      delete [] data_ ;
}

void cbmImage_t :: setPixel
   ( unsigned x,       // dots from left
     unsigned y )      // steps from top
{
   if( x < actualWidth_ )
   {
      if( y < actualHeight_ )
      {
         unsigned ySeg  = y / rowsPerSegment_ ;
         unsigned yOffs = y % rowsPerSegment_ ;

         // each segment has header, data, and trailer.
         unsigned char * const segStart = data_ + ySeg*bytesPerSegment_ ;
         unsigned char * segData = segStart + headerLen ;

         // 
         // each segment of bytes contains one column of data
         // high order bit is top row
         //
// printf( "yOffs 0x%x\n", yOffs );
         int rowBytesInSeg = ((ySeg < (numSegments_-1)) ? rowsPerSegment_ : rowsLastSegment_) /8;
         int maxBytes = actualWidth_ * rowBytesInSeg;
         int bytes = ( yOffs / 8 );
         unsigned char bitOffs = yOffs % 8 ;
         if( x )
         {
            bytes += x * rowBytesInSeg;
// printf( "xOffs == %u\n", xOffs );
         }
         if (bytes >= maxBytes) {
            printf( "maxBytes: %i, tried:%i, x:%i, y:%i\n",maxBytes,bytes,x,y );
            bytes = maxBytes-1;
         }
         unsigned char const mask = 0x80 >> bitOffs ;
         *(segData+bytes) |= mask ;
// printf( "segment offset %u, mask 0x%02x\n", segData-segStart, mask );
      }
   }
}

#ifdef MODULETEST
int main( int argc, char const * const argv[] )
{
   cbmImage_t image( 800, 600 );
   printf( "allocated image, length == %u\n", image.getLength() );
   printf( "specWidth_ == %u\n", image.specWidth_ );   // app-specified width
   printf( "specHeight_ == %u\n", image.specHeight_ );
   printf( "actualWidth_ == %u\n", image.actualWidth_ ); // padded to byte boundary
   printf( "actualHeight_ == %u\n", image.actualHeight_ );
   printf( "rowsPerSegment_ == %u\n", image.rowsPerSegment_ );
   printf( "numSegments_ == %u\n", image.numSegments_ );
   printf( "bytesPerSegment_ == %u\n", image.bytesPerSegment_ );

   for( unsigned x = 0 ; x < image.actualWidth_ ; x++ )
   {
      image.setPixel( x, 0 );
      image.setPixel( x, 2 );
      image.setPixel( x, 7 );
      image.setPixel( x, 8 );
      image.setPixel( x, x );
      image.setPixel( x, image.actualHeight_ - 1 );
   }

   for( unsigned y = 0 ; y < image.actualHeight_ ; y++ )
   {
      image.setPixel( 0, y );
      image.setPixel( image.actualWidth_ - 1, y );
   }

/*
   image.setPixel( 0, 0 );
   image.setPixel( 1, 0 );
   image.setPixel( 0, 1 );
*/

/*
   hexDumper_t dump( image.getData(), image.getLength() );
   while( dump.nextLine() )
      printf( "%s\n", dump.getLine() );
*/

   int const fd = open( "/dev/usb/lp0", O_WRONLY );
   if( 0 <= fd )
   {
      printf( "printer port opened\n" );
      int const numWritten = write( fd, image.getData(), image.getLength() );
      printf( "wrote %u bytes\n", numWritten );
      write( fd, "\n\n\n\n", 4 );
      close( fd );
   }
   else
      perror( "/dev/usb/lp0" );

   return 0 ;
}
#endif

