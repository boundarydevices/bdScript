/*
 * Module jsImage.cpp
 *
 * This module defines the JavaScript routines
 * which display images as described and declared 
 * in jsImage.h
 *
 *
 * Change History : 
 *
 * $Log: jsImage.cpp,v $
 * Revision 1.2  2002-10-15 05:00:14  ericn
 * -added imageJPEG routine
 *
 * Revision 1.1  2002/10/13 16:32:25  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsImage.h"
#include "libpng12/png.h"
#include "fbDev.h"
#include "../boundary1/hexDump.h"

extern "C" {
#include <jpeglib.h>
};

typedef struct {
   png_bytep   data_ ;
   size_t      length_ ;
} pngData_t ;

void pngRead( png_structp png, 
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

static JSBool
imagePNG(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   //
   // need at least data, plus optional x and y
   //
   if( ( ( 1 == argc ) || ( 3 == argc ) )
       &&
       JSVAL_IS_STRING( argv[0] ) 
       &&
       ( ( 1 == argc ) 
         || 
         ( JSVAL_IS_INT( argv[1] )
           &&
           JSVAL_IS_INT( argv[2] ) ) ) )   // second and third parameters must be ints
   {
      JSString *str = JS_ValueToString( cx, argv[0] );
      if( str )
      {
         unsigned startX ;
         unsigned startY ;
         if( 1 == argc )
         {
            startX = 0 ;
            startY = 0 ;
         }
         else
         {
            startX = JSVAL_TO_INT( argv[1] );
            startY = JSVAL_TO_INT( argv[2] );
         }

         png_bytep pngData = (png_bytep)JS_GetStringBytes( str );
         unsigned const pngLength = JS_GetStringLength(str);
         printf( "have image string : length %u\n", pngLength );
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

//                              printf( "allocated PNG structures\n" );
                              int interlace_type, compression_type, filter_type;
                              png_uint_32 width, height;
                              int bit_depth, color_type;

                              png_get_IHDR( png_ptr, info_ptr, &width, &height,
                                            &bit_depth, &color_type, &interlace_type,
                                            &compression_type, &filter_type);
//                              printf( "width %u, height %u, bits %u\n", width, height, bit_depth );
//                              printf( "   color type %d, interlace %d, compression %d, filter %d\n",
//                                          color_type, interlace_type, compression_type, filter_type);
                              if( PNG_COLOR_MASK_PALETTE == ( PNG_COLOR_MASK_PALETTE & color_type ) )
                                 png_set_palette_to_rgb( png_ptr );

                              png_read_update_info(png_ptr, info_ptr);

                              png_bytep *row_pointers = new png_bytep[height];
                              unsigned const rowBytes = png_get_rowbytes( png_ptr, info_ptr );
//                              printf( "   %u bytes per row\n", rowBytes );

                              for( unsigned row = 0; row < height; row++ )
                              {
                                 row_pointers[row] = (png_bytep)png_malloc( png_ptr, rowBytes );
                              }
                              
                              png_read_image( png_ptr, row_pointers );

//                              printf( "read image successfully\n" );

                              fbDevice_t &fb = getFB();
                              if( startY + height > fb.getHeight() )
                              {
                                 printf( "truncate to screen height %u\n", fb.getHeight() );
                                 height = fb.getHeight() - startY ;
                              }

                              if( startX + width > fb.getWidth() )
                              {
                                 printf( "truncate to screen width %u\n", fb.getWidth() );
                                 width = fb.getWidth() - startX ;
                              }

//                              printf( "writing %u x %u pixels\n", width, height );
                              for( unsigned row = 0 ; row < height ; row++ )
                              {
                                 unsigned char const *imgRow = row_pointers[row];
                                 for( unsigned column = 0 ; column < width ; column++, imgRow += 3 )
                                 {
                                    unsigned short const color = fb.get16( imgRow[0], imgRow[1], imgRow[2] );
                                    fb.getPixel( startX + column, startY + row ) = color ;
                                 }
                              }

                              for( unsigned row = 0; row < height; row++ )
                                 png_free( png_ptr, row_pointers[row] );
                              
                              delete [] row_pointers ;
                           }
                           else
                           {
                              fprintf( stderr, "Internal PNG error\n" );
                           }

                           png_destroy_read_struct( &png_ptr, &info_ptr, &end_info );
                        }
                     }
                     else
                        png_destroy_read_struct( &png_ptr, 0, 0 );
                  }
               }
            }
            else
               printf( "Not a PNG file\n" );
         }
         else
         {
            printf( "PNG too short!\n" );
         }
      } // retrieved string

      *rval = JSVAL_FALSE ;
      return JS_TRUE ;

   } // need at least two params

   return JS_FALSE ;
}

//
// used differently than pngData_t.
//
// libjpeg keeps track of the ptr and length, but may want to rewind,
// so this structure is kept constant and the internals of jpeg_source_mgr
// are changed to point somewhere within.
// 
//
typedef struct {
   JOCTET const *data_ ;
   size_t        length_ ;
   size_t        numRead_ ;
} jpegSrc_t ;

static void jpg_init_source( j_decompress_ptr cinfo )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   pSrc->numRead_ = 0 ;
printf( "init_source\n" );
}

static boolean jpg_fill_input_buffer( j_decompress_ptr cinfo )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   cinfo->src->bytes_in_buffer = pSrc->length_ - pSrc->numRead_ ;
   cinfo->src->next_input_byte = pSrc->data_ + pSrc->numRead_ ;
   pSrc->numRead_ = cinfo->src->bytes_in_buffer ;
printf( "fill input\n" );
   return ( 0 < cinfo->src->bytes_in_buffer );
}

static void jpg_skip_input_data( j_decompress_ptr cinfo, long num_bytes )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   unsigned left = pSrc->length_ - pSrc->numRead_ ;
   if( left > num_bytes )
      num_bytes = left ;
   pSrc->numRead_ += num_bytes ;
printf( "skip input : %u/%ld\n", pSrc->numRead_, num_bytes );
}

static boolean jpg_resync_to_restart( j_decompress_ptr cinfo, int desired )
{
printf( "resync to restart\n" );
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   pSrc->numRead_ = 0 ;
}

static void jpg_term_source( j_decompress_ptr cinfo )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   // nothing to do
}

static JSBool
imageJPEG( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   //
   // need at least data, plus optional x and y
   //
   if( ( ( 1 == argc ) || ( 3 == argc ) )
       &&
       JSVAL_IS_STRING( argv[0] ) 
       &&
       ( ( 1 == argc ) 
         || 
         ( JSVAL_IS_INT( argv[1] )
           &&
           JSVAL_IS_INT( argv[2] ) ) ) )   // second and third parameters must be ints
   {
      JSString *str = JS_ValueToString( cx, argv[0] );
      if( str )
      {
         unsigned startX ;
         unsigned startY ;
         if( 1 == argc )
         {
            startX = 0 ;
            startY = 0 ;
         }
         else
         {
            startX = JSVAL_TO_INT( argv[1] );
            startY = JSVAL_TO_INT( argv[2] );
         }

         JOCTET const *jpegData = (JOCTET *)JS_GetStringBytes( str );
         unsigned const jpegLength = JS_GetStringLength(str);
         printf( "display %u bytes of JPEG %p at %u,%u\n", jpegLength, jpegData, startX, startY );
         
         struct jpeg_decompress_struct cinfo;
         struct jpeg_error_mgr jerr;
         cinfo.err = jpeg_std_error(&jerr);
         jpeg_create_decompress(&cinfo);
         
         jpegSrc_t jpgSrc ;
         jpgSrc.data_   = jpegData ;
         jpgSrc.length_ = jpegLength ;
         jpgSrc.numRead_ = 0 ;

         cinfo.client_data = &jpgSrc ;

         jpeg_source_mgr srcMgr ;
         memset( &srcMgr, 0, sizeof( srcMgr ) );
         srcMgr.next_input_byte  = jpgSrc.data_ ; /* => next byte to read from buffer */
         srcMgr.bytes_in_buffer  = jpgSrc.length_ ;	/* # of bytes remaining in buffer */
         
         srcMgr.init_source         = jpg_init_source ;
         srcMgr.fill_input_buffer   = jpg_fill_input_buffer ;
         srcMgr.skip_input_data     = jpg_skip_input_data ;
         srcMgr.resync_to_restart   = jpg_resync_to_restart ;
         srcMgr.term_source         = jpg_term_source ;
         cinfo.src = &srcMgr ;
     
         jpeg_read_header(&cinfo, TRUE);
         cinfo.out_color_space = JCS_RGB ;
         jpeg_start_decompress(&cinfo);
         printf( "started decompress\n" );
         printf( "width %u, height %u\n", cinfo.output_width, cinfo.output_height );
         printf( "output components %u\n", cinfo.output_components );
         int row_stride = cinfo.output_width * cinfo.output_components;
         printf( "row stride %d\n", row_stride );

         JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)( (j_common_ptr)&cinfo,
                                                         JPOOL_IMAGE,
                                                         row_stride, 1);
      
         // allocate image
         unsigned imageWidth  = cinfo.output_width;
         unsigned imageHeight = cinfo.output_height;
         JSAMPLE * pixels = new JSAMPLE[imageWidth * imageHeight * 3 * sizeof( JSAMPLE )];
      
         unsigned drawWidth  = imageWidth ;
         unsigned drawHeight = imageHeight ;

         fbDevice_t &fb = getFB();
         printf( "opened framebuffer\n" );
         if( startY + drawHeight > fb.getHeight() )
         {
            printf( "truncate to screen drawHeight %u\n", fb.getHeight() );
            drawHeight = fb.getHeight() - startY ;
         }

         if( startX + drawWidth > fb.getWidth() )
         {
            printf( "truncate to screen drawWidth %u\n", fb.getWidth() );
            drawWidth = fb.getWidth() - startX ;
         }
//                              printf( "writing %u x %u pixels\n", imageWidth, imageHeight );

         printf( "image %u x %u, %u x %u\n", imageWidth, imageHeight, drawWidth, drawHeight );
         // read the scanlines
         for( unsigned row = 0 ; row < imageHeight ; row++ )
         {
            unsigned numRead = jpeg_read_scanlines( &cinfo, buffer, 1);

            if( row < drawHeight )
            {
/*
               if( 5 > row )
               {
                  printf( "row %u/%u, stride %d\n", row, cinfo.output_scanline, row_stride );
                  hexDumper_t dump( buffer[0], row_stride );
                  while( dump.nextLine() )
                     printf( "%s\n", dump.getLine() );
               }
*/
               unsigned char const *nextOut = buffer[0];
               for( unsigned column = 0; column < drawWidth; ++column ) 
               {
                  unsigned char red, green, blue ;
                  if( cinfo.output_components == 1 ) 
                  {   
                     red = green = blue = *nextOut++ ;
                  }
                  else
                  {
                     red   = *nextOut++ ;
                     green = *nextOut++ ;
                     blue  = *nextOut++ ;
                  }
                  
                  unsigned short const color = ( cinfo.output_components == 1 ) 
                                               ? fb.get16( *nextOut, 
                                                           *nextOut, 
                                                           *nextOut )
                                               : fb.get16( nextOut[0], 
                                                           nextOut[1], 
                                                           nextOut[2] );
                  fb.getPixel( startX + column, startY + row ) = color ;
               }
            }
         }

         jpeg_finish_decompress(&cinfo);

         jpeg_destroy_decompress(&cinfo);

      } // retrieved string

      *rval = JSVAL_FALSE ;
      return JS_TRUE ;

   } // need at least two params

   return JS_FALSE ;
}

static JSFunctionSpec image_functions[] = {
    {"imagePNG",         imagePNG,        1 },
    {"imageJPEG",        imageJPEG,       1 },
    {0}
};


bool initJSImage( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, image_functions);
}

