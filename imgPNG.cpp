/*
 * Module imgPNG.cpp
 *
 * This module defines the imagePNG() routine as declared
 * in imgPNG.h.
 *
 *
 * Change History : 
 *
 * $Log: imgPNG.cpp,v $
 * Revision 1.4  2002-11-23 16:05:26  ericn
 * -added alpha channel
 *
 * Revision 1.3  2002/11/20 00:39:22  ericn
 * -modified to catch errors
 *
 * Revision 1.2  2002/11/11 04:29:14  ericn
 * -moved headers
 *
 * Revision 1.1  2002/10/31 02:13:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "imgPNG.h"
#include <png.h>
#include "fbDev.h"
#include <string.h>

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

#ifdef __STANDALONE__

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

bool imagePNG( void const    *inData,     // input
               unsigned long  inSize,     // input
               void const    *&pixData,   // output
               unsigned short &width,     // output
               unsigned short &height,    // output
               void const    *&alpha )    // output : 0 if none, delete [] when done
{
   pixData = alpha = 0 ; width = height = 0 ;

   png_bytep pngData = (png_bytep)inData ;
   unsigned const pngLength = inSize ;
   if( 8 < pngLength )
   {
      if( 0 == png_sig_cmp( pngData, 0, pngLength ) )
      {
         png_structp png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
         {
            if( png_ptr )
            {
               png_infop info_ptr = png_create_info_struct(png_ptr);
               if( info_ptr )
               {
                  png_infop end_info = png_create_info_struct(png_ptr);
                  if( end_info )
                  {
                     if( 0 == setjmp( png_jmpbuf( png_ptr ) ) )
                     {
                        pngData_t pngFile ;
                        pngFile.data_   = pngData ;
                        pngFile.length_ = pngLength ;
                        
                        png_set_read_fn( png_ptr, &pngFile, pngRead );

                        png_read_info(png_ptr, info_ptr);
#ifdef __STANDALONE__
dumpPNGInfo( info_ptr );
#endif
                        int interlace_type, compression_type, filter_type;
                        png_uint_32 lWidth, lHeight;
                        int bit_depth, color_type;

                        png_get_IHDR( png_ptr, info_ptr, &lWidth, &lHeight,
                                      &bit_depth, &color_type, &interlace_type,
                                      &compression_type, &filter_type);
                        if( PNG_COLOR_MASK_PALETTE == ( PNG_COLOR_MASK_PALETTE & color_type ) )
                           png_set_palette_to_rgb( png_ptr );

                        png_read_update_info(png_ptr, info_ptr);

                        png_bytep *const row_pointers = new png_bytep[lHeight];
                        unsigned const rowBytes = png_get_rowbytes( png_ptr, info_ptr );

                        for( unsigned row = 0; row < lHeight; row++ )
                        {
                           row_pointers[row] = (png_bytep)png_malloc( png_ptr, rowBytes );
                        }
                        
                        png_read_image( png_ptr, row_pointers );

                        width  = (unsigned short)lWidth ;
                        height = (unsigned short)lHeight ;

                        fbDevice_t &fb = getFB();

                        unsigned char *const alphaChannel = ( color_type & PNG_COLOR_MASK_ALPHA )
                                                            ? new unsigned char [ width*height ]
                                                            : 0 ;

                        unsigned short *const pixMap = new unsigned short [ width*height ];
                        for( unsigned row = 0 ; row < height ; row++ )
                        {
                           unsigned char const *imgRow = row_pointers[row];
                           for( unsigned column = 0 ; column < width ; column++, imgRow += info_ptr->channels )
                           {
                              pixMap[row*width+column] = fb.get16( imgRow[0], imgRow[1], imgRow[2] );
                              if( alphaChannel )
                                 alphaChannel[row*width+column] = imgRow[3];
                           }
                        }

                        pixData = pixMap ;
                        alpha   = alphaChannel ;

                        for( unsigned row = 0; row < lHeight ; row++ )
                           png_free( png_ptr, row_pointers[row] );
                        
                        delete [] row_pointers ;
                     }
                     else
                        fprintf( stderr, "Internal PNG error\n" );

                     png_destroy_read_struct( &png_ptr, &info_ptr, &end_info );
                  }
               }
               else
                  png_destroy_read_struct( &png_ptr, 0, 0 );
            }
         }
      }
      else
         fprintf( stderr, "Not a PNG file\n" );
   }
   else
      fprintf( stderr, "PNG too short!\n" );

   return 0 != pixData ;

}



#ifdef __STANDALONE__

#include "hexDump.h"
#include "memFile.h"

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         printf( "%lu bytes read at address %p\n", fIn.getLength(), fIn.getData() );
         void const *pixData ;
         void const *alpha ;
         unsigned short width ;
         unsigned short height ;
         if( imagePNG( fIn.getData(), fIn.getLength(),
                       pixData,
                       width,
                       height,
                       alpha ) )
         {
            printf( "image: %u x %u pixels\n", width, height );
            unsigned long size = (unsigned long)width * height * 2 ;
//            hexDumper_t dump( pixData, size );
//            while( dump.nextLine() )
//               printf( "%s\n", dump.getLine() );
            if( alpha )
            {
               printf( "has alpha channel\n" );
               hexDumper_t dumpAlpha( alpha, width*height );
               while( dumpAlpha.nextLine() )
                  printf( "%s\n", dumpAlpha.getLine() );
            }
         }
         else
            fprintf( stderr, "Error converting image\n" );
      }
      else
         fprintf( stderr, "Error %s opening %s\n", fIn.getError(), argv[1] );
   }
   else
      fprintf( stderr, "Usage : imgPNG fileName\n" );

   return 0 ;
}

#endif
