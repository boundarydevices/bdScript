/*
 * Module pngToPS.cpp
 *
 * This module defines the pngToPostscript() routine as
 * declared in pngToPS.h
 *
 *
 * Change History : 
 *
 * $Log: pngToPS.cpp,v $
 * Revision 1.1  2006-10-29 21:59:10  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "pngToPS.h"
#include <png.h>
#include <zlib.h>

typedef struct {
   png_bytep   data_ ;
   size_t      length_ ;
} pngData_t ;

static void pngRead( png_structp png, 
                     png_bytep   data, 
                     png_size_t  len )
{
   pngData_t *pData = (pngData_t *)png->io_ptr ;
   if( len <= pData->length_ )
   {
      memcpy( data, pData->data_, len );
      pData->data_ += len ;
      pData->length_ -= len ;
   }
   else
      png_error( png, "short read");
}

#ifdef MODULETEST

static void dumpPNGInfo( png_infop pi )
{
   printf( "width %u\n", pi->width );       /* width of image in pixels (from IHDR) */
   printf( "height %u\n", pi->height );      /* height of image in pixels (from IHDR) */
   printf( "valid %u\n", pi->valid );       /* valid chunk data (see PNG_INFO_ below) */
   printf( "rowbytes %u\n", pi->rowbytes );    /* bytes needed to hold an untransformed row */
   printf( "palette %u\n", pi->palette );      /* array of color values (valid & PNG_INFO_PLTE) */
   printf( "num_palette %u\n", pi->num_palette ); /* number of color entries in "palette" (PLTE) */
   printf( "num_trans %u\n", pi->num_trans );   /* number of transparent palette color (tRNS) */
   printf( "bit_depth %u\n", pi->bit_depth );      /* 1, 2, 4, 8, or 16 bits/channel (from IHDR) */
   printf( "color_type %u\n", pi->color_type );     /* see PNG_COLOR_TYPE_ below (from IHDR) */
   printf( "compression_type %u\n", pi->compression_type ); /* must be PNG_COMPRESSION_TYPE_BASE (IHDR) */
   printf( "filter_type %u\n", pi->filter_type );    /* must be PNG_FILTER_TYPE_BASE (from IHDR) */
   printf( "interlace_type %u\n", pi->interlace_type ); /* One of PNG_INTERLACE_NONE, PNG_INTERLACE_ADAM7 */

   printf( "channels %u\n", pi->channels );       /* number of data channels per pixel (1, 2, 3, 4) */
   printf( "pixel_depth %u\n", pi->pixel_depth );    /* number of bits per pixel */
   printf( "spare_byte %u\n", pi->spare_byte );     /* to align the data, and for future use */

}
#endif

static void doDeflate( z_stream &stream )
{
   int must_continue = 1 ;
   do {
      must_continue = 0 ;
      int err = deflate(&stream, 0);
      if( Z_OK == err ){
         must_continue = ( 0 != stream.avail_in );
      }
      else
         break ;
   } while( must_continue );
}

bool pngToPostscript
      ( void const         *inData,
        unsigned            pngLength,
        postscript_output_t outHandler,
        void               *outParam )
{
   bool worked = false ;
   unsigned width, height ;

   png_bytep pngData = (png_bytep)inData ;
   if( 8 < pngLength )
   {
      if( 0 == png_sig_cmp( pngData, 0, pngLength ) )
      {
         png_structp png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
         if( png_ptr )
         {
            png_infop info_ptr = png_create_info_struct(png_ptr);
            if( info_ptr )
            {
               if( 0 == setjmp( png_jmpbuf( png_ptr ) ) )
               {
                  pngData_t pngFile ;
                  pngFile.data_   = pngData ;
                  pngFile.length_ = pngLength ;

                  png_set_read_fn( png_ptr, &pngFile, pngRead );

                  png_read_info(png_ptr, info_ptr);
#ifdef MODULETEST
dumpPNGInfo( info_ptr );
#endif
                  int interlace_type, compression_type, filter_type;
                  png_uint_32 lWidth, lHeight;
                  int bit_depth, color_type;

                  png_get_IHDR( png_ptr, info_ptr, &lWidth, &lHeight,
                                &bit_depth, &color_type, &interlace_type,
                                &compression_type, &filter_type);
                  if( PNG_COLOR_MASK_PALETTE == ( PNG_COLOR_MASK_PALETTE & color_type ) ){
                     png_set_palette_to_rgb( png_ptr );
                  }
                  else {
                  }

                  png_read_update_info(png_ptr, info_ptr);

                  png_bytep *const row_pointers = new png_bytep[lHeight];
                  unsigned const rowBytes = png_get_rowbytes( png_ptr, info_ptr );
                  for( unsigned row = 0; row < lHeight; row++ )
                  {
                     row_pointers[row] = (png_bytep)png_malloc( png_ptr, rowBytes );
                  }

                  png_read_image( png_ptr, row_pointers );

                  z_stream stream ;
                  memset( &stream, 0, sizeof(stream) );

                  unsigned const outBufferLen = 16384 ;
                  unsigned char * const outBuffer = new unsigned char [outBufferLen];

                  stream.next_out = outBuffer ;
	          stream.avail_out = outBufferLen ;

                  if( Z_OK == deflateInit(&stream,Z_DEFAULT_COMPRESSION)) {
                     width  = (unsigned short)lWidth ;
                     height = (unsigned short)lHeight ;
                     unsigned const bytesPerOutRow = width*3 ;

                     if( color_type & PNG_COLOR_MASK_ALPHA ){
                        unsigned char *const rgbLine = new unsigned char [bytesPerOutRow]; // R, G, B bytes
                        worked = true ;
                        for( unsigned row = 0 ; worked && ( row < height ); row++ )
                        {
                           unsigned char const *imgRow = row_pointers[row];
                           unsigned char *nextOut = rgbLine ;
                           
                           for( unsigned column = 0 ; column < width ; column++, nextOut += 3, imgRow += 4 )
                           {
                              nextOut[0] = imgRow[0];
                              nextOut[1] = imgRow[1];
                              nextOut[2] = imgRow[2];
                           }
   
                           // zip this row
                           stream.next_in = rgbLine ;
                           stream.avail_in = bytesPerOutRow ;

                           doDeflate( stream );

                           unsigned const numOut = ( outBufferLen - stream.avail_out );
                           for( unsigned i = 0 ; worked && ( i < numOut ); i++ )
                              worked = outHandler( outBuffer[i], outParam );
                           stream.next_out  = outBuffer ;
                           stream.avail_out = outBufferLen ;
                        } // for each row
      
                        delete [] rgbLine ;
                     }
                     else {
                        worked = true ;
                        for( unsigned row = 0 ; worked && ( row < height ); row++ )
                        {
                           // zip this row
                           stream.next_in  = row_pointers[row] ;
                           stream.avail_in = bytesPerOutRow ;
                           doDeflate( stream );

                           unsigned const numOut = ( outBufferLen - stream.avail_out );
                           for( unsigned i = 0 ; worked && ( i < numOut ); i++ )
                              worked = outHandler( outBuffer[i], outParam );
                           stream.next_out  = outBuffer ;
                           stream.avail_out = outBufferLen ;
                        } // for each row
                     } // no alpha channel. use row_pointers directly

                     if( worked ){
                        bool must_continue ;
                        do {
                           int err = deflate(&stream, Z_FINISH);
                           must_continue = (Z_OK == err);
                           unsigned const numOut = ( outBufferLen - stream.avail_out );
                           for( unsigned i = 0 ; worked && ( i < numOut ); i++ )
                              worked = outHandler( outBuffer[i], outParam );
                        } while( worked && must_continue );
                     }

                     deflateEnd(&stream);
                  }
                  else
                     fprintf( stderr, "deflateInit error %s\n", stream.msg );

                  delete [] outBuffer ;

                  for( unsigned row = 0; row < lHeight ; row++ )
                     png_free( png_ptr, row_pointers[row] );
                  
                  delete [] row_pointers ;
               }
               else
                  fprintf( stderr, "Internal PNG error\n" );
            }

            png_destroy_read_struct( &png_ptr, 0, 0 );
         }
         else
            fprintf( stderr, "Error creating png_read\n" );
      }
      else
         fprintf( stderr, "Not a PNG file\n" );
   }
   else
      fprintf( stderr, "PNG too short!\n" );

   return worked ;
}

#ifdef MODULETEST_PNGTOPS
#include <stdio.h>
#include "memFile.h"
#include "ascii85.h"

static int outPos = 0 ;

static bool stdOutput( char  outchar, void *opaque )
{
   FILE *fOut = (FILE *)opaque ;
   fwrite( &outchar, 1, 1, fOut );
   if( ( ++outPos > 63 )
       ||
       ( ( 1 == outPos ) && ( '%' == outchar ) ) )
   {
      fputc( '\n', fOut );
      outPos = 0 ;
   }

   return true ;
}

static bool a85Output( char  outchar, void *opaque )
{
   ascii85_t &a85 = *(ascii85_t *)opaque ;
   return a85.convert( &outchar, 1 );
}

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc ){
      memFile_t fIn( argv[1] );

      if( fIn.worked() ){
         ascii85_t a85( stdOutput, stdout );

         if( pngToPostscript( fIn.getData(), fIn.getLength(),
                              a85Output, &a85 ) )
         {
            fprintf( stderr, "Conversion complete\n" );
            if( a85.flush() ){
               fprintf( stderr, "output written\n" );
            }
            else
               fprintf( stderr, "flush error\n" );
         }
         else
            fprintf( stderr, "Conversion error\n" );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s inFile\n", argv[0] );

   return 0 ;
}

#endif
