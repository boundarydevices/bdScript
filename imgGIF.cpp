/*
 * Module imgGIF.cpp
 *
 * This module defines the imgGIF() routine
 * as declared in imgGIF.h
 *
 *
 * Change History : 
 *
 * $Log: imgGIF.cpp,v $
 * Revision 1.2  2002-11-23 16:05:01  ericn
 * -added placeholder for alpha channel
 *
 * Revision 1.1  2002/10/31 02:13:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "imgGIF.h"
extern "C" {
#include <gif_lib.h>
};
#include "fbDev.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>

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

static void gif2PixMap( ColorMapObject const &map,
                        SavedImage const     &image,
                        int                   bgColor,
                        unsigned short      *&pixMap,
                        unsigned short       &width,
                        unsigned short       &height )
{
   unsigned top    = image.ImageDesc.Top ; // screen position
   unsigned left   = image.ImageDesc.Left ; // screen position
   height = (unsigned short)( top + image.ImageDesc.Height );
   width  = (unsigned short)( left + image.ImageDesc.Width );

   unsigned short const * const rgb16 = (unsigned short *)map.Colors ;
   
   pixMap = new unsigned short [ height*width ];
   unsigned short const bg16 = rgb16[bgColor];
   for( unsigned y = 0 ; y < top ; y++ )
   {
      for( unsigned x = 0 ; x < width ; x++ )
         pixMap[y*width+x] = bg16 ;
   }
      
   for( unsigned x = 0 ; x < left ; x++ )
   {
      for( unsigned y = 0 ; y < height ; y++ )
      {
         pixMap[y*width+x] = bg16 ;
      }
   }

   char const *raster = image.RasterBits ;
   if( 0 == image.ImageDesc.Interlace )
   {
      for( int row = 0 ; row < image.ImageDesc.Height ; row++ )
      {
         unsigned const screenY = top + row ;
         for( int column = 0 ; column < image.ImageDesc.Width ; column++, raster++ )
         {
            unsigned const screenX = left + column ;
            char const colorIdx = *raster ;
            pixMap[screenY*width+screenX] = rgb16[colorIdx];
         } // for each column
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
            for( int column = 0 ; column < image.ImageDesc.Width ; column++, raster++ )
            {
               unsigned const screenX = left + column ;
               char const colorIdx = *raster ;
               pixMap[screenY*width+screenX] = rgb16[colorIdx];
            }
         } // for each row in this pass
      } // for each of 4 passes
   } // interlaced
}

bool imageGIF( void const    *inData,     // input
               unsigned long  inSize,     // input
               void const    *&pixData,   // output
               unsigned short &width,     // output
               unsigned short &height,    // output
               void const    *&alpha )    // output : 0 if none, delete [] when done
{
   pixData = alpha = 0 ; width = height = 0 ;

   gifSrc_t src ;
   src.data_    = (GifByteType *)inData ;
   src.length_  = inSize ;
   src.numRead_ = 0 ;

   GifFileType *fGIF = DGifOpen( &src, gifRead );
   if( fGIF )
   {   
      if( GIF_OK == DGifSlurp( fGIF ) )
      {
         fbDevice_t &fb = getFB();

         if( fGIF->SColorMap )
            fixupColorMap( fb, *fGIF->SColorMap );
         if( 1 <= fGIF->ImageCount )
         {
            SavedImage const *image = fGIF->SavedImages ;
            ColorMapObject *colorMap ;
            if( image->ImageDesc.ColorMap )
            {
               fixupColorMap( fb, *image->ImageDesc.ColorMap );
               colorMap = image->ImageDesc.ColorMap ;
            }
            else 
               colorMap = fGIF->SColorMap ;
            
            if( colorMap )
            {
               unsigned short *pixMap ;
               gif2PixMap( *colorMap, *image, fGIF->SBackGroundColor,
                           pixMap, width, height );
               pixData = pixMap ;
            }
            else
               fprintf( stderr, "Invalid color map\n" );
         }
         else
            fprintf( stderr, "No images\n" );
      }
      DGifCloseFile( fGIF );
   }
   else
      fprintf( stderr, "Error reading GIF\n" );

   return ( 0 != pixData );
}

