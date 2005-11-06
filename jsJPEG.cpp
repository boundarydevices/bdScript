/*
 * Module jsJPEG.cpp
 *
 * This module defines the initialization routine
 * for jsJPEG as declared and described in jsJPEG.h
 *
 *
 * Change History : 
 *
 * $Log: jsJPEG.cpp,v $
 * Revision 1.2  2005-11-06 00:49:32  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.1  2003/03/22 03:50:10  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsJPEG.h"

#include <unistd.h>
#include "js/jscntxt.h"
#include <assert.h>
#include <string.h>
#include "fbDev.h"
#include "hexDump.h"

extern "C" {
#include <jpeglib.h>
};

#define BYTESPERMEMCHUNK 4096

struct memChunk_t {
   unsigned long  length_ ; // bytes used so far
   memChunk_t    *next_ ;   // next chunk
   JOCTET         data_[BYTESPERMEMCHUNK-8];
};

struct memDest_t {
   struct       jpeg_destination_mgr pub; /* public fields */
   memChunk_t  *chunkHead_ ;
   memChunk_t  *chunkTail_ ;
};

typedef memDest_t  *memDestPtr_t ;

#if 0
static void dumpcinfo( jpeg_compress_struct const &cinfo )
{
   hexDumper_t dump( &cinfo, sizeof( cinfo ) );
   while( dump.nextLine() )
      printf( "%s\n", dump.getLine() );
   fflush( stdout );
}

static void dumpMemDest( memDest_t const &md )
{
   hexDumper_t dump( &md, sizeof( md ) );
   while( dump.nextLine() )
      printf( "%s\n", dump.getLine() );
   fflush( stdout );
}
#endif

/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */
static void init_destination (j_compress_ptr cinfo)
{
   memDestPtr_t dest = (memDestPtr_t) cinfo->dest;
   
   assert( 0 == dest->chunkHead_ );

   /* Allocate first memChunk */
   dest->chunkHead_ = dest->chunkTail_ = new memChunk_t ;
   memset( dest->chunkHead_, 0, sizeof( *dest->chunkHead_ ) );
   dest->pub.next_output_byte = dest->chunkHead_->data_ ;
   dest->pub.free_in_buffer   = sizeof( dest->chunkHead_->data_ );
}


/*
 * Empty the output buffer --- called whenever buffer fills up.
 */

static boolean empty_output_buffer (j_compress_ptr cinfo)
{
   memDestPtr_t const dest = (memDestPtr_t) cinfo->dest;
   
   assert( 0 != dest->chunkTail_ );

   //
   // free_in_buffer member doesn't seem to be filled in
   //
   dest->chunkTail_->length_ = sizeof( dest->chunkTail_->data_ ); //  - dest->pub.free_in_buffer ;

   memChunk_t * const next = new memChunk_t ;
   memset( next, 0, sizeof( *next ) );
   dest->chunkTail_->next_ = next ;
   dest->chunkTail_ = next ;
   
   dest->pub.next_output_byte = next->data_ ;
   dest->pub.free_in_buffer   = sizeof( next->data_ );

   return TRUE;
}


/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */
static void term_destination (j_compress_ptr cinfo)
{
   //
   // just account for data used
   //
   memDest_t  *const dest = (memDestPtr_t) cinfo->dest ;
   memChunk_t  *tail = dest->chunkTail_ ;
   assert( 0 != tail );   
   assert( sizeof( tail->data_ ) >= dest->pub.free_in_buffer );
   assert( tail->data_ <= dest->pub.next_output_byte );
   assert( tail->data_ + sizeof( tail->data_ ) >= dest->pub.next_output_byte + dest->pub.free_in_buffer );

   tail->length_ = sizeof( tail->data_ ) - dest->pub.free_in_buffer ;
}

/*
 * Prepare for output to a chunked memory stream.
 */
void jpeg_mem_dest( j_compress_ptr cinfo )
{
   assert( 0 == cinfo->dest );
   cinfo->dest = (struct jpeg_destination_mgr *)
                 (*cinfo->mem->alloc_small)
                     ( (j_common_ptr) cinfo, 
                       JPOOL_IMAGE,
		       sizeof(memDest_t)
                     );
   memDest_t *dest = (memDest_t *) cinfo->dest ;
   dest->pub.init_destination    = init_destination ;
   dest->pub.empty_output_buffer = empty_output_buffer ;
   dest->pub.term_destination    = term_destination ;
   dest->chunkHead_ = 
   dest->chunkTail_ = 0 ;
}

static JSBool
jsImgToJPEG( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) && JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *const rhObj = JSVAL_TO_OBJECT( argv[0] );
      jsval widthVal, heightVal, dataVal ;

      if( JS_GetProperty( cx, rhObj, "width", &widthVal )
          &&
          JSVAL_IS_INT( widthVal )
          &&
          JS_GetProperty( cx, rhObj, "height", &heightVal )
          &&
          JSVAL_IS_INT( heightVal )
          &&
          JS_GetProperty( cx, rhObj, "pixBuf", &dataVal )
          &&
          JSVAL_IS_STRING( dataVal ) )
      {
         int width  = JSVAL_TO_INT( widthVal ); 
         int height = JSVAL_TO_INT( heightVal );
         JSString *pixStr = JSVAL_TO_STRING( dataVal );
         unsigned short const *const pixMap = (unsigned short *)JS_GetStringBytes( pixStr );
         if( JS_GetStringLength( pixStr ) == width * height * sizeof( pixMap[0] ) )
         {
            struct jpeg_compress_struct cinfo;
            struct jpeg_error_mgr jerr;
            cinfo.err = jpeg_std_error(&jerr);
            jpeg_create_compress( &cinfo );
            cinfo.in_color_space = JCS_RGB; /* arbitrary guess */
            jpeg_set_defaults(&cinfo);
            cinfo.dct_method = JDCT_IFAST;
            cinfo.in_color_space = JCS_RGB;
            cinfo.input_components = 3;
            cinfo.data_precision = 8;
            cinfo.image_width = (JDIMENSION) width;
            cinfo.image_height = (JDIMENSION) height;
            jpeg_default_colorspace(&cinfo);
            jpeg_mem_dest( &cinfo );
            jpeg_start_compress( &cinfo, TRUE );

            unsigned const row_stride = 3*sizeof(JSAMPLE)*width; // RGB
            JSAMPARRAY const buffer = (*cinfo.mem->alloc_sarray)( (j_common_ptr)&cinfo,
                                                                  JPOOL_IMAGE,
                                                                  row_stride, 1 );
            //
            // loop through scan lines here
            //
            unsigned short const *currentRow = pixMap ;
            for( int row = 0 ; row < height ; row++ )
            {
               JSAMPLE *nextOut = buffer[0];
               unsigned short const *nextIn = currentRow ;
               currentRow += width ;
               for( int col = 0 ; col < width ; col++ )
               {
                  unsigned short const rgb16 = *nextIn++ ;
                  *nextOut++ = fbDevice_t::getRed( rgb16 );
                  *nextOut++ = fbDevice_t::getGreen( rgb16 );
                  *nextOut++ = fbDevice_t::getBlue( rgb16 );
               } // for each column
               jpeg_write_scanlines( &cinfo, buffer, 1 );
            } // for each row

            memChunk_t const *const firstChunk = ( (memDest_t *)cinfo.dest )->chunkHead_ ;
            jpeg_finish_compress( &cinfo );
            memChunk_t const *ch = firstChunk ;
            unsigned totalBytes = 0 ;
            while( ch )
            {
               totalBytes += ch->length_ ;
               ch = ch->next_ ;
            }

            void *const stringMem = JS_malloc( cx, totalBytes );
            if( stringMem )
            {
               JSAMPLE *nextOut = (JSAMPLE *)stringMem ;
               
               for( ch = firstChunk ; 0 != ch ;  )
               {
                  memcpy( nextOut, ch->data_, ch->length_ );
                  nextOut += ch->length_ ;
                  memChunk_t const *temp = ch ;
                  ch = ch->next_ ;
                  delete temp ;
               }
               assert( nextOut == (JSAMPLE *)stringMem + totalBytes );
               JSString *sReturn = JS_NewString( cx, (char *)stringMem, totalBytes );
               if( sReturn )
               {
                  *rval = STRING_TO_JSVAL( sReturn );
               }
               else
                  JS_ReportError( cx, "JPEGNewString" );
            }
            else
               JS_ReportError( cx, "jpegMalloc" );

printf( "total bytes:%u\n", totalBytes ); fflush( stdout );

            jpeg_destroy_compress( &cinfo );
         }
         else
            JS_ReportError( cx, "Invalid pixmap" );
      }
      else
         JS_ReportError( cx, "Invalid image" );
   }
   else
      JS_ReportError( cx, "Usage: imgToJPEG( img )" );

   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"imgToJPEG",           jsImgToJPEG,          0 },
    {0}
};

bool initJSJPEG( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}

