/*
 * Module jpegToYUV.cpp
 *
 * This module defines the jpegToYUV() routine as declared
 * in jpegToYUV.h
 *
 *
 * Change History : 
 *
 * $Log: jpegToYUV.cpp,v $
 * Revision 1.2  2008-09-30 23:32:40  ericn
 * remove unused variable (fb)
 *
 * Revision 1.1  2008-06-08 19:52:13  ericn
 * add jpeg->YUV support
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */


#include "jpegToYUV.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "fbDev.h"

extern "C" {
#include <jpeglib.h>
};


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
   if( left < (unsigned)num_bytes )
      num_bytes = left ;
   pSrc->numRead_ += num_bytes ;
   cinfo->src->next_input_byte += (size_t) num_bytes;
   cinfo->src->bytes_in_buffer -= (size_t) num_bytes;
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
   // nothing to do
}

bool jpegToYUV( void const    *inData,     // input
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
   cinfo.out_color_space = JCS_YCbCr ;
   cinfo.dct_method = JDCT_IFAST;   
   
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

   unsigned char * const pixMap = new unsigned char[ cinfo.output_height*cinfo.output_width*2 ];
   unsigned char *yuvOut = pixMap ;
   
   // read the scanlines
   for( unsigned row = 0 ; row < cinfo.output_height ; row++ )
   {
      jpeg_read_scanlines( &cinfo, buffer, 1);
      unsigned char const *nextOut = buffer[0];

      for( unsigned column = 0; column < cinfo.output_width; ++column )
      {
         unsigned char y, u, v ;
         if( cinfo.output_components == 1 )
         {
            y = u = v = *nextOut++ ;
         }
         else
         {
            y = *nextOut++ ;
            u = *nextOut++ ;
            v = *nextOut++ ;
         }

         *yuvOut++ = y ;
         if( column & 1 )
            *yuvOut++ = v ;
         else
            *yuvOut++  = u ;
      }
   }

   pixData = pixMap ;
   width   = cinfo.output_width ;
   height  = cinfo.output_height ;

   jpeg_finish_decompress(&cinfo);

   jpeg_destroy_decompress(&cinfo);

   return true ;
}

#ifdef STANDALONE_JPEG_TO_YUV
#include "pxaYUV.h"
#include "memFile.h"

int main( int argc, char const * const argv[] )
{
   printf( "Hello, %s\n", argv[0] );
   if( 2 <= argc ){
      memFile_t fIn( argv[1] );
      if( fIn.worked() ){
         printf( "%s: %lu bytes\n", argv[1], fIn.getLength() );
         void const *pixData ;
         unsigned short width, height ;

         if(jpegToYUV(fIn.getData(), fIn.getLength(), pixData, width, height) ){
            printf( "%s: %ux%u\n", argv[1], width, height );
            pxaYUV_t yuv(0, 0, width, height);
            yuv.writeInterleaved( (unsigned char const *)pixData );
            char inBuf[80];
            fgets(inBuf,sizeof(inBuf),stdin);
         }
      }
      else
         perror( argv[1] );
   }
   return 0 ;
}

#endif

#ifdef IMX_JPEG_TO_YUV
#include "fb2_overlay.h"
#include "fbDev.h"
#include "memFile.h"
#include <linux/videodev2.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>

static fb2_overlay_t *fb2_overlay = 0 ;

static fb2_overlay_t &getOverlay(unsigned x, unsigned y, unsigned w, unsigned h)
{
    static unsigned prevx = -1U ;
    static unsigned prevy = -1U ;
    static unsigned prevw = -1U ;
    static unsigned prevh = -1U ;
    if(fb2_overlay && (x == prevx) && (y == prevy) && (w == prevw) && (h == prevh) ){
        return *fb2_overlay ;
    }
    else if( fb2_overlay ){
        delete fb2_overlay ;
        fb2_overlay = 0 ;
    }
    fb2_overlay = new fb2_overlay_t(x,y,w,h,255,V4L2_PIX_FMT_YUV420);
    if( fb2_overlay && fb2_overlay->isOpen() ){
        printf( "created new overlay: %u:%u %ux%u\n", x, y, w, h );
        prevx = x ;
        prevy = y ;
        prevw = w ; 
        prevh = h ;
        return *fb2_overlay ;
    }
    else {
        perror("overlay");
        exit(1);
    }
}

static void closeOverlay( void )
{
   printf( "closing overlay\n");
   if( fb2_overlay ){
       delete fb2_overlay ;
       fb2_overlay = 0 ;
   }
}

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>(%d)\r\n", signo );
   exit(1);
}

static void showImage(void const *pixData, unsigned short width, unsigned short height)
{
     fbDevice_t &fb = getFB();
     if( (height > fb.getHeight()) || (width > fb.getWidth()) ){
         printf( "too big\n" );
         return ;
     }
     unsigned x = (fb.getWidth()-width)>>1 ;
     unsigned y = (fb.getHeight()-height)>>1 ;
     printf( "%ux%u->%ux%u @%u:%u\n", width, height, fb.getWidth(), fb.getHeight(), x, y );
     fb2_overlay_t &overlay = getOverlay(x,y,width,height);
     printf( "outSize: %u\n", overlay.getMemSize());
     unsigned char *yOut = (unsigned char *)overlay.getMem();
     unsigned char *uOut = yOut + (width*height*2);
     unsigned char *vOut = uOut + (width*height)/2 ;
     printf( "offsets: 0x%x, 0x%x\n", uOut-yOut, vOut-yOut );
     unsigned char const *inData = (unsigned char *)pixData ;
     for( unsigned row = 0 ; row < height ; row++){
       for( unsigned col = 0 ; col < width ; col++ ){
          *yOut++ = *inData++ ;
          unsigned char c = 
             *inData++ ;
          if( row & 1 ){
             if(0 == (col&1))
                *uOut++ = c ;
             else
                *vOut++ = c ;
          }
       }
     }
}

static void trimCtrl(char *buf){
        char *tail = buf+strlen(buf);
        // trim trailing <CR> if needed
        while ( tail > buf ) {
                --tail ;
                if ( iscntrl(*tail) ) {
                        *tail = '\0' ;
                }
                else
                        break;
        }
}

static void showFile(char const *fileName)
{
      memFile_t fIn( fileName );
      if( fIn.worked() ){
         printf( "%s: %lu bytes\n", fileName, fIn.getLength() );
         void const *pixData ;
         unsigned short width, height ;

         if(jpegToYUV(fIn.getData(), fIn.getLength(), pixData, width, height) ){
             showImage(pixData,width,height);
         }
      }
      else
         perror( fileName );
}

int main( int argc, char const * const argv[] )
{
   printf( "Hello, %s\n", argv[0] );
   atexit( closeOverlay );
   signal( SIGINT, ctrlcHandler );
   signal( SIGTERM, ctrlcHandler );
   signal( SIGKILL, ctrlcHandler );
   if( 2 <= argc ){
        for( int arg = 1 ; arg < argc ; arg++ ){
            showFile(argv[arg]);
        }
   }

    char inBuf[80];
    while(fgets(inBuf,sizeof(inBuf),stdin)){
        trimCtrl(inBuf);
        showFile(inBuf);
    }
   
    return 0 ;
}

#endif
