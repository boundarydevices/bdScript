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
 * Revision 1.1  2002-10-31 02:13:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "imgPNG.h"
#include <libpng12/png.h>
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

bool imagePNG( void const    *inData,     // input
               unsigned long  inSize,     // input
               void const    *&pixData,   // output
               unsigned short &width,     // output
               unsigned short &height )   // output
{
   pixData = 0 ; width = height = 0 ;

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

                        int interlace_type, compression_type, filter_type;
                        png_uint_32 lWidth, lHeight;
                        int bit_depth, color_type;

                        png_get_IHDR( png_ptr, info_ptr, &lWidth, &lHeight,
                                      &bit_depth, &color_type, &interlace_type,
                                      &compression_type, &filter_type);
                        if( PNG_COLOR_MASK_PALETTE == ( PNG_COLOR_MASK_PALETTE & color_type ) )
                           png_set_palette_to_rgb( png_ptr );

                        png_read_update_info(png_ptr, info_ptr);

                        png_bytep *row_pointers = new png_bytep[lHeight];
                        unsigned const rowBytes = png_get_rowbytes( png_ptr, info_ptr );

                        for( unsigned row = 0; row < lHeight; row++ )
                        {
                           row_pointers[row] = (png_bytep)png_malloc( png_ptr, rowBytes );
                        }
                        
                        png_read_image( png_ptr, row_pointers );

                        width  = (unsigned short)lWidth ;
                        height = (unsigned short)lHeight ;

                        fbDevice_t &fb = getFB();

                        unsigned short *const pixMap = new unsigned short [ width*height ];
                        for( unsigned row = 0 ; row < height ; row++ )
                        {
                           unsigned char const *imgRow = row_pointers[row];
                           for( unsigned column = 0 ; column < width ; column++, imgRow += 3 )
                           {
                              pixMap[row*width+column] = fb.get16( imgRow[0], imgRow[1], imgRow[2] );
                           }
                        }

                        pixData = pixMap ;

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

