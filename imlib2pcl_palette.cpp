#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <Imlib2.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>
#include <set>
#include "palette.h"

#define ESC "\x1b"

// can't be part of the format because of embedded NULLs
static char const imagePreamble0[] = {
   ESC "*v6W"        // Configure Image ID???
   "\x00"            // color space:   0 == Device RGB (1 == Device CMY, 2 == Colorimetric RGB, 3 == CIE L*a*b, 4 == Luminance/Chrominance)
   "\x01"            // pixel encoding:   3 == Direct by Pixel (0 == indexed by plane, 1 == indexed by pixel, 2 == direct by plane)
   "\x08"
   "\x08"
   "\x08"
   "\x08"
};

static char const palEntryFmt[] = {
   ESC "*v%03uA"
   ESC "*v%03uB"
   ESC "*v%03uC"
   ESC "*v%03uI"
};

static char const imagePreambleFmt[] = {
   ESC "*r3F"         //  set rotation to page orientation
   ESC "*r%uT"        //  set number of raster lines
   ESC "*r%uS"        //  set number of pixels / line
   ESC "*r3A"         //  start raster graphics at current position, use destination resolution for scaling
   ESC "*b0Y"         //  move by 0 lines
   ESC "*b5M"         //  compression mode five (adaptivexrow)
   ESC "*b0000000000W" // max 10-digit length
};

static char const imageTrailer[] = {
   ESC "*r0C"         // end raster graphics
};

enum rowMode_e {
   uncompressed,     // 0
   rle,              // 1
   tiffByte,         // 2
   delta,            // 3
   emptyRow,         // 4
   duplicateRow,     // 5
   numRowModes
};

static char const * const modeNames[numRowModes] = {
   "uncompressed",     // 0
   "rle",              // 1
   "tiffByte",         // 2
   "delta",            // 3
   "emptyRow",         // 4
   "duplicateRow"      // 5
};

//
// Returns number of bytes used to make an RLE row
// Returns a value > rowBytes if it fails to compress
//
static unsigned makeRLE( unsigned char const *inRow,      // input: row to encode
                         unsigned             rowBytes,   // input: bytes of input
                         unsigned char       *rleRow )    // output
{
   unsigned char const * const inEnd = inRow+rowBytes ;
   unsigned char * const outEnd = rleRow+rowBytes-2 ;

   unsigned char *nextOut = rleRow ;
   while( (inRow < inEnd)
          &&
          (nextOut < outEnd) )
   {
      unsigned char const matchChar = *inRow++ ;
      unsigned count = 1 ;
      while( ( 256 > count )
             && 
             ( inRow < inEnd ) ){
         unsigned char nextChar = *inRow ;
         if( nextChar == matchChar ){
            count++ ;
            inRow++ ;
         }
         else
            break ;
      }

      *nextOut++ = (count-1);
      *nextOut++ = matchChar ;
   }

   if( nextOut < outEnd )
   {
      return nextOut-rleRow ;
   }
   else
      return rowBytes+1 ;
}

static bool testRLE( unsigned char const *inRaw,
                     unsigned             rawBytes,
                     unsigned char const *inRLE,
                     unsigned             rleBytes )
{
   assert( 0 == (rleBytes&1) );

   unsigned char const *rawEnd = inRaw + rawBytes ;
   while( 0 < rleBytes ){
      unsigned count = (*inRLE++) + 1 ;
      unsigned char value = *inRLE++ ;
      rleBytes -= 2 ;
      for( unsigned i = 0 ; i < count ; i++ ){
         unsigned char raw = *inRaw++ ;
         if( raw != value ){
            printf( "mismatch %02x/%02x\n", raw, value );
            return false ;
         }
      }
   }

   if( rawEnd != inRaw )
      printf( "tail-end mismatch %p/%p\n", rawEnd, inRaw );
   return rawEnd == inRaw ;
}

//
// Returns number of bytes used to make a TIFF row
// Returns a value > rowBytes if it fails to compress
//
static unsigned makeTIFF( unsigned char const *inRow,      // input: row to encode
                          unsigned             rowBytes,   // input: bytes of input
                          unsigned char       *tiffRow )   // output
{
   unsigned char const * const inEnd = inRow+rowBytes ;
   unsigned char const *nextIn = inRow ; 

   unsigned char *const outEnd = tiffRow + rowBytes - 2 ;
   unsigned char *nextOut = tiffRow ;

   //
   // Loop over each input byte
   //    count matches
   //    store matches using -repeatCount/byte or (count-1)/multibytes
   //
   while( nextIn < inEnd ){
      unsigned char const *start = nextIn ;
      unsigned char match = *nextIn++ ;
      while( ( match == *nextIn ) && ( nextIn < inEnd ) )
         nextIn++ ;

      unsigned numMatched = nextIn - start ;
      if( 2 < numMatched ){
         //
         // numMatched may be more than we can store...
         // Loop outputting all of them
         //
         while( 2 < numMatched ){
            unsigned count = ( numMatched > 127 ) ? 127 : numMatched ;
            unsigned spaceLeft = outEnd-nextOut ;
            if( 2 < spaceLeft ){
               *nextOut++ = (unsigned char)(0-(count-1));
               *nextOut++ = match ;
            }
            else
               return rowBytes + 1 ; // out of space
            numMatched -= count ;
         }
         
         //
         // could have a leftover byte or two here (repeat of 128, for instance)
         // reset the input to include the last instance of a repeated character
         //
         if( 0 < numMatched )
            nextIn -= numMatched ; 
      } // use repeat count
      else {
         //
         // find end of literal string (1st set of 3 identical or end of input)
         //
         unsigned repeated = 0 ;
         while( ( repeated < 3 ) && ( nextIn < inEnd ) )
         {
            unsigned char c = *nextIn++ ;
            if( c == match ){
               repeated++ ;
            }
            else {
               repeated = 0 ;
               match = c ;
            }
         }
         
         if( nextIn < inEnd ){
            // found a set of 3 repeating... back up
            nextIn -= 3 ;
         }

         unsigned count = nextIn-start ;
         while( 0 < count ){
            unsigned num = ( count > 128 ) ? 128 : count ;
            unsigned spaceLeft = outEnd-nextOut ;
            
            if( num+1 < spaceLeft ){
               *nextOut++ = (num-1);
               memcpy( nextOut, start, num );
               start += num ;
               nextOut += num ;
            }
            else
               return rowBytes + 1 ; // out of space

            count -= num ;
         }
      } // output literal string
   }

   return nextOut-tiffRow ;

}

static bool testTIFF( unsigned char const *inRaw,
                      unsigned             rawBytes,
                      unsigned char const *inTIFF,
                      unsigned             tiffBytes )
{
   unsigned char const *rawEnd = inRaw + rawBytes ;
   while( 0 < tiffBytes ){
      int count = (signed char)(*inTIFF++);
      tiffBytes-- ;

      if( 0 <= count ){
         count++ ;
//         printf( "%u bytes of literal text\n", count );
         unsigned numLeft = rawEnd-inRaw ;
         if( numLeft >= count ){
            if( 0 == memcmp( inTIFF, inRaw, count ) ){
               inTIFF += count ;
               inRaw += count ;
               tiffBytes -= count ;
            }
            else {
               printf( "TIFF Literal Mismatch:\n" );
               printf( "TIFF: " );
               for( unsigned i = 0 ; i < count ; i++ )
                  printf( "%02x ", inTIFF[i] );
               printf( "\n"
                       "RAW:  " );
               for( unsigned i = 0 ; i < count ; i++ )
                  printf( "%02x ", inRaw[i] );
               printf( "\n" );
               return false ;
            }
         }
         else {
            printf( "TIFF ran off end-of-input\n" );
            return false ;
         }
      } // literal text
      else {
         unsigned char const value = *inTIFF++ ;
         --tiffBytes ;

         count = (0-count)+1 ;
//         printf( "%u repeated bytes [%02x]\n", count, value );
         unsigned numLeft = rawEnd-inRaw ;
         if( numLeft >= count ){
            for( unsigned i = 0 ; i < count ; i++ ){
               if( *inRaw++ != value ){
                  printf( "TIFF repeat mismatch: %02x/%02x\n", inRaw[-1], value );
                  return false ;
               }
            }
         }
         else {
            printf( "TIFF repeat off end-of-input\n" );
            return false ;
         }
      } // repeated byte
   }

   return true ;
}

static void printBytes( unsigned char const *bytes, unsigned count )
{
   printf( "(%04x)", count );
   for( unsigned i = 0 ; i < count ; i++ )
      printf( "%02X ", *bytes++ );
   printf( "\n" );
}


//
// Returns the number of bytes used to produce a delta row
// Returns a value > rowBytes if it fails to compress
//
static unsigned makeDeltaRow( unsigned char const *prevRow,    // input:   delta from this
                              unsigned char const *curRow,     // input:           to this
                              unsigned             rowBytes,   // input:
                              unsigned char       *deltaRow )  // output
{
   unsigned char *const xorred = new unsigned char [rowBytes];
   unsigned numOutput = rowBytes+1 ;
   for( unsigned i = 0 ; i < rowBytes ; i++ )
   {
      xorred[i] = prevRow[i] ^ curRow[i];
   }

   unsigned char const *nextIn = xorred ;
   unsigned char const * const endIn = xorred+rowBytes ;

//   printf( "prev: " ); printBytes( prevRow, rowBytes );
//   printf( "cur:  " ); printBytes( curRow, rowBytes );
   
   unsigned bytesUsed = 0 ;
   numOutput = 0 ;
   while( nextIn < endIn ){
      // skip matching bytes
      while( ( 0 == *nextIn )
             &&
             ( nextIn < endIn ) )
      {
         nextIn++ ;
      }

      if( nextIn < endIn )
      {
         //
         // find 2 matching bytes or end of input
         //
         unsigned matchCount = 0 ;
         unsigned const diffStart = nextIn-xorred ;
         while( ( nextIn < endIn )
                &&
                ( 2 > matchCount ) )
         {
            if( *nextIn )
               matchCount = 0 ;
            else
               matchCount++ ;
            nextIn++ ;
         }

         unsigned diffEnd = nextIn-xorred ;
         if( nextIn < endIn ){
            nextIn  -= 2 ; // skip matching bytes for 
            diffEnd -= 2 ;
         }

         unsigned numLiteral = diffEnd-diffStart ;
         bytesUsed += numLiteral + (numLiteral+7)/8 ;

//         printf( "literal: %u bytes at [%u..%u]\n", diffEnd-diffStart, diffStart, diffEnd );
      }
   }

   unsigned same = 0 ;
   unsigned diff = 0 ;
   for( unsigned i = 0 ; i < rowBytes ; i++ )
      if( xorred[i] )
         diff++ ;
      else
         same++ ;

//   printf( "%u bytes: %u same/%u diff, %u used\n", rowBytes, same, diff, bytesUsed );

bail:
   delete [] xorred ;
   numOutput = bytesUsed ;
   numOutput = rowBytes+1 ;
   return numOutput ;
}

static std::set<unsigned long> colorsUsed ;

int main(int argc, char **argv)
{
   if (argc != 3){
      fprintf( stderr, "Usage: %s infile outfile\n", argv[0] );
      exit(1);
   }

   char const *inFileName = argv[1];
   Imlib_Image image = imlib_load_image(inFileName);
   if (!image){
      fprintf( stderr, "Error loading image %s\n", inFileName );
      exit(1);
   }

   char const * const outFileName = argv[2];
   FILE *fOut = fopen( outFileName, "wb" );
   if( !fOut ){
      perror( outFileName );
      exit(1);
   }

   imlib_context_set_image(image);

   unsigned const h = imlib_image_get_height();
   unsigned const w = imlib_image_get_width();
   unsigned const rowBytes = w ;

   fprintf( stderr, "image %s: %ux%u\n", inFileName, w, h );

   fwrite( imagePreamble0, sizeof(imagePreamble0)-1, 1, fOut );

   //
   // Walk colors once to build palette
   //
   palette_t palette ;
   for( unsigned row = 0 ; row < h ; row++ )
   {
      for( unsigned column = 0 ; column < w ; column++ )
      {
         Imlib_Color color ;
         imlib_image_query_pixel( column, row, &color );
         palette.nextIn( color.red, color.green, color.blue );
      }
   }
   palette.buildPalette();

   //
   // define palette to printer
   //
   for( unsigned palEntry = 0 ; palEntry < palette.numPaletteEntries(); palEntry++ )
   {
      unsigned char r, g, b ;
      palette.getRGB( palEntry, r, g, b );

      char cPalEntry[sizeof(palEntryFmt)];
      int entryLen = snprintf( cPalEntry, sizeof( cPalEntry ), palEntryFmt, r,g,b,palEntry );
      fwrite( cPalEntry, 1, entryLen, fOut );
   }

   fprintf( fOut, imagePreambleFmt, h, w );
   
   fflush(fOut);
   long int const preambleEndPos = ftell(fOut);
   long int const imgLenPos = preambleEndPos-11 ;
   
   double maxY = -2000000.0 ;
   double minY = 2000000.0 ;
   double maxU = -2000000.0 ;
   double minU = 2000000.0 ;
   double maxV = -2000000.0 ;
   double minV = 2000000.0 ;
   int maxR = INT_MIN,
       maxG = INT_MIN,
       maxB = INT_MIN ;

   unsigned char * const prevRow = new unsigned char [rowBytes];
   unsigned char * const curRow = new unsigned char [rowBytes];
   unsigned char * const rleRow = new unsigned char [rowBytes];
   unsigned char * const tiffRow = new unsigned char [rowBytes];
   unsigned char * const deltaRow = new unsigned char [rowBytes];

   //
   // walk again to get data
   //
   unsigned savings[numRowModes] = { 0 };
   unsigned emptyRowSavings = 0 ;
   unsigned dupRowSavings = 0 ;
   for( unsigned row = 0 ; row < h ; row++ )
   {
      unsigned char *nextOut = curRow ;
      for( unsigned column = 0 ; column < w ; column++ )
      {
         Imlib_Color color ;
         imlib_image_query_pixel( column, row, &color );
         unsigned char paletteIndex ;
         bool worked = palette.getIndex( color.red, color.green, color.blue, paletteIndex );
         assert( worked );

         *nextOut++ = paletteIndex ;

         unsigned char r, g, b ;
         palette.getRGB( paletteIndex, r, g, b );
/*
         *nextOut++ = r;
         *nextOut++ = g;
         *nextOut++ = b;
*/         
         unsigned long const rgb = (r << 16) | (g << 8) | b ;

         colorsUsed.insert(rgb);
/*
         *nextOut++ = color.red & 0xFF ;
         *nextOut++ = color.green & 0xFF ;
         *nextOut++ = color.blue & 0xFF ;
*/
         if( color.red > maxR )
            maxR = color.red ;
         if( color.green > maxG )
            maxG = color.green ;
         if( color.blue > maxB )
            maxB = color.blue ;
/*
         unsigned long const rgb = (color.red << 16) 
                                 | (color.green << 8)
                                 | color.blue ;
         colorsUsed.insert(rgb);

//         double R = color.red/255.0 ;
//         double G = color.green/255.0 ;
//         double B = color.blue/255.0 ;
//         double Y =  (0.257 * R) + (0.504 * G) + (0.098 * B);
//         double V =  (0.439 * R) - (0.368 * G) - (0.071 * B) + 128 ;
//         double U = -(0.148 * R) - (0.291 * G) + (0.439 * B) + 128 ;
         double Y = (( ( 66 * color.red + 129 * color.green + 25 * color.blue + 128) >> 8) + 16);
         double U = ( ( -38 * color.red - 74 * color.green + 112 * color.blue + 128) >> 8) + 128;
         double V = ( ( 112 * color.red - 94 * color.green - 18 * color.blue + 128) >> 8) + 128;

         if( Y > maxY )
            maxY = Y ;
         if( Y < minY )
            minY = Y ;

         if( U > maxU )
            maxU = U ;
         if( U < minU )
            minU = U ;

         if( V > maxV )
            maxV = V ;
         if( V < minV )
            minV = V ;

         unsigned char y = (unsigned char)Y ;
         unsigned char u = (unsigned char)U ;
         unsigned char v = (unsigned char)V ;
         *nextOut++ = y ;
         *nextOut++ = u ;
         *nextOut++ = v ;
*/         
      }
      assert( nextOut-curRow == rowBytes );

      rowMode_e mode ;
      if( ( 0xFF == curRow[0] )
          &&
          ( 0 == memcmp(curRow,curRow+1,rowBytes-1) ) ){
         unsigned char rowPreamble[3];
   
         rowPreamble[0] = emptyRow ;
         rowPreamble[1] = 0 ;
         rowPreamble[2] = 1 ; // one row
         fwrite( rowPreamble, 1, sizeof(rowPreamble), fOut );
         emptyRowSavings += rowBytes;
      }
      else if( ( 0 != row )
               &&
               ( 0 == memcmp( curRow, prevRow, rowBytes ) ) )
      {
         unsigned char rowPreamble[3];
   
         rowPreamble[0] = duplicateRow ;
         rowPreamble[1] = 0 ;
         rowPreamble[2] = 1 ; // one row
         fwrite( rowPreamble, 1, sizeof(rowPreamble), fOut );
         dupRowSavings += rowBytes;
      }
      else {
//         printf( "raw: " ); printBytes( curRow, rowBytes );

         unsigned const numRLE = makeRLE( curRow, rowBytes, rleRow ); // rowBytes+1 ; // 
         unsigned const numTIFF = makeTIFF( curRow, rowBytes, tiffRow );
         unsigned const numDelta = ( 0 < row ) 
                                   ? makeDeltaRow( prevRow, curRow, rowBytes, deltaRow )
                                   : rowBytes+1 ;

         unsigned char const *src = curRow ;
         rowMode_e mode = uncompressed ;
         unsigned byteCount = rowBytes ;
         if( numRLE < byteCount ){
//            printf( "rle: " ); printBytes( rleRow, numRLE );
            if( !testRLE( curRow, rowBytes, rleRow, numRLE ) ){
               printf( "Invalid RLE on row %u\n", row );
               exit(1);
            }
            byteCount = numRLE ;
            src = rleRow ;
            mode = rle ;
         }

         if( numTIFF < byteCount ){
            if( !testTIFF( curRow, rowBytes, tiffRow, numTIFF ) ){
               printf( "Invalid TIFF data on row %u\n", row );
               printf( "raw:  " ); printBytes( curRow, rowBytes );
               printf( "tiff: " ); printBytes( tiffRow, numTIFF );
//               exit(1);
            }
            byteCount = numTIFF ;
            src = tiffRow ;
            mode = tiffByte ;
         }
         if( numDelta < byteCount ){
            byteCount = numDelta ;
            src       = deltaRow ;
            mode      = delta ;
         }

         unsigned char rowPreamble[3];

         if( uncompressed != mode ){
//            printf( "mode %s: %u bytes saved\n", modeNames[mode], rowBytes-byteCount );
            savings[mode] += rowBytes-byteCount ;
         }
         rowPreamble[0] = mode ;
         rowPreamble[1] = (byteCount >> 8) & 0xff ;
         rowPreamble[2] = byteCount & 0xff ;
         fwrite( rowPreamble, sizeof(rowPreamble), 1, fOut );
         fwrite( src, 1, byteCount, fOut );
      }
      memcpy( prevRow, curRow, rowBytes );
   }
   long int const rasterEndPos = ftell(fOut);

   fwrite( imageTrailer, 1, sizeof(imageTrailer)-1, fOut );

   printf( "Y range [%f..%f]\n", minY, maxY );
   printf( "U range [%f..%f]\n", minU, maxU );
   printf( "V range [%f..%f]\n", minV, maxV );
   printf( "RGB range: [%u,%u,%u]\n", maxR, maxG, maxB );
   long int const rasterLen = rasterEndPos-preambleEndPos ;
   fprintf( stderr, "%lu bytes of raster data: store at offset %lx/%lx\n", rasterLen, imgLenPos,preambleEndPos );
   for( unsigned i = 0 ; i < numRowModes; i++ ){
      if( savings[i] )
         printf( "%s: %u bytes saved\n", modeNames[i], savings[i] );
   }
   if( emptyRowSavings )
      printf( "empty rows: %u bytes saved\n", emptyRowSavings );
   if( dupRowSavings )
      printf( "dup rows: %u bytes saved\n", dupRowSavings );

   char imgLen[11];
   snprintf(imgLen,sizeof(imgLen),"%010lu", rasterLen);
   
   fflush(fOut);
   fseek(fOut,imgLenPos,SEEK_SET);
   fwrite(imgLen,1,10,fOut);
   fclose(fOut);

   printf( "color usage == %u\n", imlib_get_color_usage() );
   imlib_set_color_usage(256);
   printf( "color usage now %u\n", imlib_get_color_usage() );
   printf( "colors used == %u\n", colorsUsed.size() );
   return 0 ;
}
