/*
 * Module imageToPS.cpp
 *
 * This module defines the imageToPS() routine as declared in 
 * imageToPS.h
 *
 *
 * Change History : 
 *
 * $Log: imageToPS.cpp,v $
 * Revision 1.2  2006-11-22 17:26:54  ericn
 * -add newline to image output
 *
 * Revision 1.1  2006/10/29 21:59:10  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "imageToPS.h"
#include "imageInfo.h"
#include "pngToPS.h"
#include <stdio.h>
#include "ascii85.h"

static char const preambleFormat[] = {
   "gsave\n"
   "/pstr %u string def\n"                // bytes/line
   "/inputf\n"
     "currentfile\n"
     "/ASCII85Decode filter\n"
     "/%s filter\n"
   "def\n"
   "%u %u translate\n"                    // output x, y
   "%u %u scale\n"                        // output width, height
   "%u %u 8\n"                            // input width, height, bit/component
   "[%d %d %d %d %d %d]\n"                // transform image->postscript coords
   "{ inputf pstr readstring pop }\n"     // data source
   "false\n"                              // !multi
   "3\n"                                  // 3 color components (R,G,B)
   "colorimage\n"
};

static char const trailerFormat[] = {
   "\n"
   "~>\n"
   "currentdict /inputf undef\n"
   "currentdict /pstr undef\n"
   "grestore\n"
};

static unsigned const trailerSize = sizeof(trailerFormat)-1 ;

#define BLOCKSIZE 16384

struct asciiBlock_t {
   image_ps_output_t outHandler_ ;
   void             *outParam_ ;
   unsigned          length_ ;
   unsigned          column_ ;
   char              data_[BLOCKSIZE];
};

static bool ascii85_buffer( char outchar, void *opaque )
{
   bool worked = true ;
   asciiBlock_t &ab = *( asciiBlock_t * )opaque ;
   ab.data_[ab.length_++] = outchar ;
   
   if( sizeof(ab.data_) <= ab.length_ + 4 ){
      worked = ab.outHandler_( ab.data_, ab.length_, ab.outParam_ );
      ab.length_ = 0 ;
   } // flush data

   if( ( 63 < ++ab.column_ )
       || 
       ( ( 1 == ab.column_ ) && ( '%' == outchar ) ) )
   {
      ab.data_[ab.length_++] = '\n' ;
      ab.column_ = 0 ;
   } // need newline

   return worked ;
}

static bool binary_buffer( char outchar, void *opaque )
{
   ascii85_t &a85 = *(ascii85_t *)opaque ;
   return a85.convert( &outchar, 1 );
}

bool imageToPS( void const        *imgData,      // input: image data
                unsigned           imgLen,       // input: bytes
                rectangle_t const &outRect,      // in printer units
                image_ps_output_t  outHandler,
                void              *outParam )
{
   imageInfo_t details ;
   if( getImageInfo( imgData, imgLen, details ) 
       &&
       ( image_t::imgGIF != details.type_ ) )
   {
      char const *filterName ;

      // see http://www.leptonica.com/affine.html
      int a, b, c, d, e, f ;      // matrix coords
      int yOffset ;
      if( image_t::imgJPEG == details.type_ )
      {
         filterName = "DCTDecode" ;
         a = details.width_ ;
         b = 0 ;
         c = 0 ;
         d = 0-details.height_ ;
         e = 0 ;
         f = details.height_ ;
         yOffset = 0 ;
      }
      else
      {
         filterName = "FlateDecode" ;
         a = details.width_ ;
         b = 0 ; 
         c = 0 ;
         d = 0-details.height_ ;
         e = 0 ;
         f = 0 ;
         yOffset = outRect.height_ ;
      }

      char preamble[2*sizeof(preambleFormat)];
      int const preLen = 
         snprintf( preamble,
                   sizeof(preamble),
                   preambleFormat,
                   3*details.width_,
                   filterName,
                   outRect.xLeft_, outRect.yTop_ + yOffset,
                   outRect.width_, outRect.height_,
                   details.width_, details.height_,
                   a, b, c, d, e, f );
      if( outHandler( preamble, preLen, outParam ) ){
         bool worked = true ;
         asciiBlock_t a85buf ;
         a85buf.outHandler_ = outHandler ;
         a85buf.outParam_   = outParam ;
         a85buf.length_     = 0 ;
         a85buf.column_     = 0 ;
         if( image_t::imgJPEG == details.type_ )
         {
            ascii85_t a85( ascii85_buffer, &a85buf );
            if( a85.convert( (char const *)imgData, imgLen ) ){
               if( a85.flush() ){
               }
               else
                  worked = false ;
            }
            else
               worked = false ;
         } // JPEG image
         else
         {
            ascii85_t a85( ascii85_buffer, &a85buf );
            worked = pngToPostscript( imgData, imgLen, binary_buffer, &a85 );
         } // PNG image

         if( worked ){
            if( 0 < a85buf.length_ ){
               worked = outHandler( a85buf.data_, a85buf.length_, outParam );
            }
            if( worked )
               worked = outHandler( trailerFormat, trailerSize, outParam );
         }

         return worked ;
      }
   } // parsed image header
   return false ;
}



#ifdef MODULETEST_IMAGETOPS

#include "memFile.h"
#include <stdlib.h>

static bool image_ps_output( char const *outData,
                             unsigned    outLength,
                             void       *opaque )
{
   fwrite( outData, 1, outLength, stdout );
   return true ;
}

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc ){
      memFile_t fIn( argv[1] );

      if( fIn.worked() ){
         rectangle_t r ;
         r.xLeft_  = ( 2 < argc ) ? strtoul( argv[2], 0, 0 ) : 0 ;
         r.yTop_   = ( 3 < argc ) ? strtoul( argv[3], 0, 0 ) : 0 ;
         r.width_  = ( 4 < argc ) ? strtoul( argv[4], 0, 0 ) : 100 ;
         r.height_ = ( 5 < argc ) ? strtoul( argv[5], 0, 0 ) : 100 ;


         if( imageToPS( fIn.getData(), fIn.getLength(),
                        r, image_ps_output, 0 ) )
         {
            fprintf( stderr, "output PS data\n" );
         }
         else
            fprintf( stderr, "%s Error getting image info\n", argv[1] );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s inFile\n", argv[0] );

   return 0 ;
}

#endif
