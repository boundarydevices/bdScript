/*
 * Module imgJPEG.cpp
 *
 * This module defines the imageJPEG() routine as
 * declared in imgJPEG.h
 *
 *
 * Change History : 
 *
 * $Log: imgJPEG.cpp,v $
 * Revision 1.3  2002-11-22 14:58:16  ericn
 * -fixed off-by-one bug, added stand-alone test
 *
 * Revision 1.2  2002/11/15 14:39:52  ericn
 * -added dummy return value
 *
 * Revision 1.1  2002/10/31 02:13:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "imgJPEG.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include <jpeglib.h>
};

#include "fbDev.h"

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
   return true ;
}

static void jpg_term_source( j_decompress_ptr cinfo )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   // nothing to do
}

bool imageJPEG( void const    *inData,     // input
                unsigned long  inSize,     // input
                void const    *&pixData,   // output
                unsigned short &width,     // output
                unsigned short &height )   // output
{
   pixData = 0 ; width = height = 0 ;
   
   JOCTET const *jpegData = (JOCTET *)inData ;
   unsigned const jpegLength = inSize ;
   
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

   JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)( (j_common_ptr)&cinfo,
                                                   JPOOL_IMAGE,
                                                   row_stride, 1);
   jpeg_start_decompress(&cinfo);

   fbDevice_t &fb = getFB();
   unsigned short * const pixMap = new unsigned short[ cinfo.output_height*cinfo.output_width ];

   // read the scanlines
   for( unsigned row = 0 ; row < cinfo.output_height ; row++ )
   {
      unsigned numRead = jpeg_read_scanlines( &cinfo, buffer, 1);
      unsigned char const *nextOut = buffer[0];

      for( unsigned column = 0; column < cinfo.output_width; ++column ) 
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
         
         unsigned short const color = fb.get16( red, green, blue );
         pixMap[row*cinfo.output_width+column] = color ;
      }
   }

   pixData = pixMap ;
   width   = cinfo.output_width ;
   height  = cinfo.output_height ;

   jpeg_finish_decompress(&cinfo);

   jpeg_destroy_decompress(&cinfo);

   return true ;
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
         unsigned short width ;
         unsigned short height ;
         if( imageJPEG( fIn.getData(), fIn.getLength(),
                        pixData,
                        width,
                        height ) )
         {

            printf( "image: %u x %u pixels\n", width, height );
            unsigned long size = (unsigned long)width * height * 2 ;
            hexDumper_t dump( pixData, size );
            while( dump.nextLine() )
               printf( "%s\n", dump.getLine() );
         }
         else
            fprintf( stderr, "Error converting image\n" );
      }
      else
         fprintf( stderr, "Error %s opening %s\n", fIn.getError(), argv[1] );
   }
   else
      fprintf( stderr, "Usage : imgJPEG fileName\n" );

   return 0 ;
}

#endif
