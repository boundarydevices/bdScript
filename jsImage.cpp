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
 * Revision 1.5  2002-10-25 13:47:28  ericn
 * -fixed memory leak, removed debug code
 *
 * Revision 1.4  2002/10/25 04:50:58  ericn
 * -removed debug statements
 *
 * Revision 1.3  2002/10/16 02:03:19  ericn
 * -Added GIF support
 *
 * Revision 1.2  2002/10/15 05:00:14  ericn
 * -added imageJPEG routine
 *
 * Revision 1.1  2002/10/13 16:32:25  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsImage.h"
#include <libpng12/png.h>
#include "fbDev.h"
#include "../boundary1/hexDump.h"

extern "C" {
#include <jpeglib.h>
#include <gif_lib.h>
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
//         printf( "have image string : length %u\n", pngLength );
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
//                                 printf( "truncate to screen height %u\n", fb.getHeight() );
                                 height = fb.getHeight() - startY ;
                              }

                              if( startX + width > fb.getWidth() )
                              {
//                                 printf( "truncate to screen width %u\n", fb.getWidth() );
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
               fprintf( stderr, "Not a PNG file\n" );
         }
         else
            fprintf( stderr, "PNG too short!\n" );
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
//printf( "init_source\n" );
}

static boolean jpg_fill_input_buffer( j_decompress_ptr cinfo )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   cinfo->src->bytes_in_buffer = pSrc->length_ - pSrc->numRead_ ;
   cinfo->src->next_input_byte = pSrc->data_ + pSrc->numRead_ ;
   pSrc->numRead_ = cinfo->src->bytes_in_buffer ;
//printf( "fill input\n" );
   return ( 0 < cinfo->src->bytes_in_buffer );
}

static void jpg_skip_input_data( j_decompress_ptr cinfo, long num_bytes )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   unsigned left = pSrc->length_ - pSrc->numRead_ ;
   if( left > num_bytes )
      num_bytes = left ;
   pSrc->numRead_ += num_bytes ;
//printf( "skip input : %u/%ld\n", pSrc->numRead_, num_bytes );
}

static boolean jpg_resync_to_restart( j_decompress_ptr cinfo, int desired )
{
//printf( "resync to restart\n" );
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
//         printf( "display %u bytes of JPEG %p at %u,%u\n", jpegLength, jpegData, startX, startY );
         
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
         
         //
         // per documentation, sample array should be allocated before
         // start_decompress() call. In order to get the image dimensions,
         // we need to call calc_output_dimensions().
         //
         jpeg_calc_output_dimensions(&cinfo);
         
         int row_stride = cinfo.output_width * cinfo.output_components;
         unsigned imageWidth  = cinfo.output_width;
         unsigned imageHeight = cinfo.output_height;
         JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)( (j_common_ptr)&cinfo,
                                                         JPOOL_IMAGE,
                                                         row_stride, 1);
         jpeg_start_decompress(&cinfo);

         // allocate image
      
         unsigned drawWidth  = imageWidth ;
         unsigned drawHeight = imageHeight ;

         fbDevice_t &fb = getFB();
         if( startY + drawHeight > fb.getHeight() )
            drawHeight = fb.getHeight() - startY ;

         if( startX + drawWidth > fb.getWidth() )
            drawWidth = fb.getWidth() - startX ;

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


typedef struct {
   GifByteType const *data_ ;
   size_t             length_ ;
   size_t             numRead_ ;
} gifSrc_t ;

static int gifRead( GifFileType *fIn, GifByteType *data, int count )
{
   gifSrc_t &src = *( gifSrc_t * )fIn->UserData ;
   unsigned const left = src.length_ - src.numRead_ ;
   if( count > left )
      count = left ;

   memcpy( data, src.data_+src.numRead_, count );
   src.numRead_ += count ;

   return count ;
}

//
// Change each color in palette once to 16-bit
//
static void fixupColorMap( fbDevice_t     &fb,
                           ColorMapObject &map )
{
   GifColorType   *rgb = map.Colors ;
   unsigned short *rgb16 = (unsigned short *)map.Colors ;
   for( unsigned i = 0 ; i < map.ColorCount ; i++, rgb++, rgb16++ )
   {
      unsigned short color = fb.get16( rgb->Red, rgb->Green, rgb->Blue );
      *rgb16 = color ;
   }
}

static int const InterlacedOffset[] = { 0, 4, 2, 1 }; /* The way Interlaced image should. */
static int const InterlacedJumps[] = { 8, 8, 4, 2 };    /* be read - offsets and jumps... */

static void displayGIF( fbDevice_t           &fb,
                        ColorMapObject const &map,
                        SavedImage const     &image,
                        unsigned              startX,
                        unsigned              startY )
{
   unsigned top    = image.ImageDesc.Top + startY ; // screen position
   unsigned left   = image.ImageDesc.Left + startX ; // screen position

   if( ( top < fb.getHeight() ) && ( left < fb.getWidth() ) )
   {
      unsigned height = image.ImageDesc.Height ;
      unsigned width  = image.ImageDesc.Width ;

      unsigned bottom = top + height ;
      if( bottom > fb.getHeight() )
         bottom = fb.getHeight();
      unsigned right = left + width ;
      if( right > fb.getWidth() )
         right = fb.getWidth();

      unsigned short const * const rgb16 = (unsigned short *)map.Colors ;
      char const *raster = image.RasterBits ;

      if( 0 == image.ImageDesc.Interlace )
      {
         for( int row = 0 ; row < image.ImageDesc.Height ; row++ )
         {
            unsigned const screenY = top + row ;
            if( screenY < fb.getHeight() )
            {
               for( int column = 0 ; column < image.ImageDesc.Width ; column++, raster++ )
               {
                  unsigned const screenX = left + column ;
                  if( screenX < fb.getWidth() )
                  {
                     char const colorIdx = *raster ;
                     fb.getPixel( screenX, screenY ) = rgb16[colorIdx];
                  } // visible
               }
            }
            else
               break;
         }
      } // non-interlaced image
      else
      {
         //
         // make 4 passes
         //
         for( unsigned i = 0; i < 4; i++ )
         {
            for( int row = InterlacedOffset[i] ; row < image.ImageDesc.Height ; row += InterlacedJumps[i] )
            {
               unsigned const screenY = top + row ;
               if( screenY < fb.getHeight() )
               {
                  for( int column = 0 ; column < image.ImageDesc.Width ; column++, raster++ )
                  {
                     unsigned const screenX = left + column ;
                     if( screenX < fb.getWidth() )
                     {
                        char const colorIdx = *raster ;
                        fb.getPixel( screenX, screenY ) = rgb16[colorIdx];
                     } // visible
                  }
               } // ? visible
               else
                  raster += image.ImageDesc.Width ;
            } // for each row in this pass
         } // for each of 4 passes
      } // interlaced

   } // room for something
}

static JSBool
imageGIF( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
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

         gifSrc_t src ;
         src.data_    = (GifByteType *)JS_GetStringBytes( str );
         src.length_  = JS_GetStringLength(str);
         src.numRead_ = 0 ;

         GifFileType *fGIF = DGifOpen( &src, gifRead );
         if( fGIF )
         {   
//            printf( "opened GIF structure\n" );
            if( GIF_OK == DGifSlurp( fGIF ) )
            {
               fbDevice_t &fb = getFB();
/*
               printf( "read GIF successfully\n" );
               printf( "   width %d, height %d\n", fGIF->SWidth, fGIF->SHeight );
               printf( "   colorRes %d\n", fGIF->SColorResolution );
               printf( "   bgColor  %d\n", fGIF->SBackGroundColor );
               printf( "   SColorMap %p\n", fGIF->SColorMap );
*/               
               if( fGIF->SColorMap )
                  fixupColorMap( fb, *fGIF->SColorMap );

/*
               printf( "   imgCount %d\n", fGIF->ImageCount );
               printf( "   Left,Top,Width,Height = %d,%d,%d,%d\n", fGIF->Image.Left, fGIF->Image.Top, fGIF->Image.Width, fGIF->Image.Height );
               printf( "   %s\n", fGIF->Image.Interlace ? "Interlaced" : "Not interlaced" );
               printf( "   Image.ColorMap %p\n", fGIF->Image.ColorMap );
*/               
               for( int i = 0 ; i < fGIF->ImageCount ; i++ )
               {
//                  printf( "   --- Image %d ---\n", i );
                  SavedImage const *image = fGIF->SavedImages + i ;
/*
                  printf( "      Left,Top,Width,Height = %d,%d,%d,%d\n", image->ImageDesc.Left, image->ImageDesc.Top, image->ImageDesc.Width, image->ImageDesc.Height );
                  printf( "      %s\n", image->ImageDesc.Interlace ? "Interlaced" : "Not interlaced" );
                  printf( "      ColorMap %p\n", image->ImageDesc.ColorMap );
*/                  
                  ColorMapObject *colorMap ;
                  if( image->ImageDesc.ColorMap )
                  {
                     fixupColorMap( fb, *image->ImageDesc.ColorMap );
                     colorMap = image->ImageDesc.ColorMap ;
                  }
                  else 
                     colorMap = fGIF->SColorMap ;

//                  printf( "      bits %p\n", image->RasterBits );
                  if( colorMap )
                  {
//                     printf( "      using map %p\n", colorMap );
                     displayGIF( fb, *colorMap, *image, startX, startY );
                  }
/*                  else
                     printf( "      no color map... can't display\n" );
*/                     

               }
            }
            DGifCloseFile( fGIF );
         }
         else
            fprintf( stderr, "Error opening GIF\n" );

      } // retrieved string

      *rval = JSVAL_FALSE ;
      return JS_TRUE ;

   } // need at least two params

   return JS_FALSE ;
}

static JSFunctionSpec image_functions[] = {
    {"imagePNG",         imagePNG,        1 },
    {"imageJPEG",        imageJPEG,       1 },
    {"imageGIF",         imageGIF,        1 },
    {0}
};


bool initJSImage( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, image_functions);
}

