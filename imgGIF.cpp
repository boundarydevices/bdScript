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
 * Revision 1.6  2008-08-23 22:01:30  ericn
 * [imgGIF] Use different STANDALONE symbol
 *
 * Revision 1.5  2007-01-11 21:32:14  ericn
 * -prevent compiler warning about signed/unsigned chars
 *
 * Revision 1.4  2006/09/24 16:21:05  ericn
 * -add anigif support (alpha quality)
 *
 * Revision 1.3  2005/11/05 20:24:44  ericn
 * -fix compiler warnings
 *
 * Revision 1.2  2002/11/23 16:05:01  ericn
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

#define DEBUGPRINT
#include "debugPrint.h"

#ifdef DEBUGPRINT
#include "hexDump.h"
#endif

typedef struct {
   GifByteType const *data_ ;
   size_t             length_ ;
   size_t             numRead_ ;
} gifSrc_t ;

static int gifRead( GifFileType *fIn, GifByteType *data, int count )
{
   gifSrc_t &src = *( gifSrc_t * )fIn->UserData ;
   unsigned const left = src.length_ - src.numRead_ ;
   if( (unsigned)count > left )
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
   for( unsigned i = 0 ; i < (unsigned)map.ColorCount ; i++, rgb++, rgb16++ )
   {
      unsigned short color = fb.get16( rgb->Red, rgb->Green, rgb->Blue );
      *rgb16 = color ;
   }
}

static unsigned short getColor( fbDevice_t           &fb,
                                ColorMapObject const &map, 
                                unsigned              idx )
{
   unsigned rgb16 = 0 ;
   if( (int)idx < map.ColorCount ){
      GifColorType const *rgb = map.Colors + idx ;
      rgb16 = fb.get16( rgb->Red, rgb->Green, rgb->Blue );
   }
   else
      fprintf( stderr, "Invalid color index %u/%u\n", idx, map.ColorCount );

   return rgb16 ;
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

   char const *raster = (char const *)image.RasterBits ;
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


static void gif2PixMap2( fbDevice_t           &fb,
                         ColorMapObject const &map,
                         SavedImage const     &image,
                         int                   bgColor,
                         unsigned short       *pixels,
                         unsigned short        width,
                         unsigned short        height )
{
   unsigned top    = image.ImageDesc.Top ; // screen position
   unsigned left   = image.ImageDesc.Left ; // screen position

   unsigned short const bg16 = ( (int)bgColor < map.ColorCount )
                               ? fb.get16( map.Colors[bgColor].Red, 
                                           map.Colors[bgColor].Green, 
                                           map.Colors[bgColor].Blue )
                               : 0 ;
   for( unsigned y = 0 ; y < top ; y++ )
   {
      for( unsigned x = 0 ; x < width ; x++ )
         pixels[y*width+x] = bg16 ;
   }
      
   for( unsigned x = 0 ; x < left ; x++ )
   {
      for( unsigned y = 0 ; y < height ; y++ )
      {
         pixels[y*width+x] = bg16 ;
      }
   }

   char const *raster = (char const *)image.RasterBits ;
   if( 0 == image.ImageDesc.Interlace )
   {
      for( int row = 0 ; row < image.ImageDesc.Height ; row++ )
      {
         unsigned const screenY = top + row ;
         for( int column = 0 ; column < image.ImageDesc.Width ; column++, raster++ )
         {
            unsigned const screenX = left + column ;
            char const colorIdx = *raster ;
            if( (int)colorIdx < map.ColorCount ){
               GifColorType const *rgb = map.Colors + colorIdx ;
               pixels[screenY*width+screenX] = fb.get16( rgb->Red, rgb->Green, rgb->Blue );
            }
            else
               pixels[screenY*width+screenX] = 0 ;
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
               if( (int)colorIdx < map.ColorCount ){
                  GifColorType const *rgb = map.Colors + colorIdx ;
                  pixels[screenY*width+screenX] = fb.get16( rgb->Red, rgb->Green, rgb->Blue );
               }
               else
                  pixels[screenY*width+screenX] = 0 ;
            }
         } // for each row in this pass
      } // for each of 4 passes
   } // interlaced
}

static bool findBounds( GifFileType *fGIF,
                        unsigned    &width,
                        unsigned    &height )
{
   width = fGIF->SWidth ; 
   height = fGIF->SHeight ;
   for( int i = 0 ; i < fGIF->ImageCount ; i++ ){
      GifImageDesc const &desc = fGIF->SavedImages[i].ImageDesc ;
      unsigned w = desc.Left+desc.Width ;
      if( w > width )
         width = w ;
      unsigned h = desc.Top+desc.Height ;
      if( h > height )
         height = h ;
   }
   return true ;
}

bool aniGIF( void const   *inData,      // input
             unsigned long inSize,      // input
             anigif_t    *&animation )  // output if return value non-zero
{
   animation = 0 ;

   gifSrc_t src ;
   src.data_    = (GifByteType *)inData ;
   src.length_  = inSize ;
   src.numRead_ = 0 ;

   GifFileType *fGIF = DGifOpen( &src, gifRead );
   if( fGIF )
   {   
      if( GIF_OK == DGifSlurp( fGIF ) )
      {
         unsigned width, height ;
         if( ( 1 <= fGIF->ImageCount )
             &&
             findBounds( fGIF, width, height ) )
         {
            fbDevice_t &fb = getFB();

            unsigned imgSize = width*height*sizeof(unsigned short);
            unsigned allocSize = (fGIF->ImageCount*sizeof(aniImage_t))
                               + sizeof(anigif_t)
                               + (fGIF->ImageCount*sizeof(aniImage_t *))
                               + (fGIF->ImageCount*imgSize);
            char *const bytes = new char [allocSize];
            animation = (anigif_t *)bytes ;
            animation->bgColor_ = getColor( fb, *fGIF->SColorMap, fGIF->SBackGroundColor );
            animation->width_ = width ;
            animation->height_ = height ;
            animation->numImages_ = fGIF->ImageCount ;
            animation->images_ = (aniImage_t **)(animation+1);
            memset( animation->images_, 0, fGIF->ImageCount*sizeof(aniImage_t *) );

            char *nextOut = (char *)( animation->images_ + fGIF->ImageCount );
            debugPrint( "%u bytes in image\n", allocSize );

            debugPrint( "%ux%u, %u colors, bg 0x%06X\n",
                        width, height,		/* Screen dimensions. */
                        fGIF->SColorResolution, 		/* How many colors can we generate? */
                        animation->bgColor_);		/* I hope you understand this one... */

            for( int i = 0 ; i < fGIF->ImageCount ; i++ ){
               struct SavedImage const &img = fGIF->SavedImages[i];
               aniImage_t *aniImg = (aniImage_t *)nextOut ;
               animation->images_[i] = aniImg ;
               memset( aniImg, 0, sizeof( *aniImg ) );
               nextOut += sizeof(aniImage_t);

               ColorMapObject *colorMap = fGIF->SColorMap ;
               if( img.ImageDesc.ColorMap )
                  colorMap = img.ImageDesc.ColorMap ;

               aniImg->xLeft_  = img.ImageDesc.Left ;
               aniImg->yTop_   = img.ImageDesc.Top ;
               aniImg->width_  = img.ImageDesc.Width ;
               aniImg->height_ = img.ImageDesc.Height ;

               if( ( ( aniImg->xLeft_ + aniImg->width_ ) <= animation->width_ )
                   &&
                   ( ( aniImg->yTop_ + aniImg->height_ ) <= animation->height_ ) ){
                  unsigned short *outPixels = (unsigned short *)aniImg->data_ ;
                  gif2PixMap2( fb, *colorMap, 
                               img,
                               fGIF->SBackGroundColor,
                               outPixels, aniImg->width_, aniImg->height_ );
               }
               else {
                  fprintf( stderr, "Invalid image width or height\n"
                                   "%u:%u %u:%u..%u:%u\n",
                           animation->width_, animation->height_,
                           aniImg->xLeft_, aniImg->yTop_,
                           aniImg->width_, aniImg->height_ );
                  memset( nextOut, 0, imgSize );
               }

               nextOut += imgSize ;
               debugPrint( "[%d] == %u:%u, %u:%u (%u) %p\n",
                       i,
                       img.ImageDesc.Left,
                       img.ImageDesc.Top,
                       img.ImageDesc.Width,
                       img.ImageDesc.Height,
                       img.ImageDesc.Interlace,
                       img.ImageDesc.ColorMap );
               debugPrint( "   %u extension blocks (%p)\n",
                       img.ExtensionBlockCount,
                       img.ExtensionBlocks );

               for( int j = 0 ; j < img.ExtensionBlockCount ; j++ ){
                  debugPrint( "   [%u] == %d, %u bytes\n", 
                          j, 
                          img.ExtensionBlocks[j].Function,
                          img.ExtensionBlocks[j].ByteCount );
                  if( GRAPHICS_EXT_FUNC_CODE == img.ExtensionBlocks[j].Function ){
#ifdef DEBUGPRINT
                     hexDumper_t dump( img.ExtensionBlocks[j].Bytes,
                                       img.ExtensionBlocks[j].ByteCount );
                     while( dump.nextLine() ){
                        debugPrint( "      %s\n", dump.getLine() );
                     }
#endif
                     if( 3 < img.ExtensionBlocks[j].ByteCount ){
                        aniImg->delay100ths_ = (img.ExtensionBlocks[j].Bytes[2] << 8) 
                                             + img.ExtensionBlocks[j].Bytes[1];
                        aniImg->hasTransparency_ = ( 0 != ( 1 && img.ExtensionBlocks[j].Bytes[0] ) );

                        if( aniImg->hasTransparency_ ){
                           if( colorMap ){
                              debugPrint( "   transparent color index %u\n", img.ExtensionBlocks[j].Bytes[3] );
                              aniImg->transparentColor_ = getColor( fb, *colorMap, img.ExtensionBlocks[j].Bytes[3] );
                           }
                           else
                              fprintf( stderr, "No color map\n" );
                        }
                        else
                           debugPrint( "   no transparency\n" );
                     }
                     else
                        fprintf( stderr, "graphics extension too short %u\n", img.ExtensionBlocks[j].ByteCount);
                  } // graphics extension
               } // walk each extension
            } // for each image
         }
         else
            fprintf( stderr, "No images\n" );
      }
      DGifCloseFile( fGIF );
   }
   else
      fprintf( stderr, "Error reading GIF\n" );

   return ( 0 != animation );
}

void anigif_t::dispose( struct anigif_t *&ag )
{
   delete [] (char *)ag ;
   ag = 0 ;
}



#ifdef IMGGIF_STANDALONE
#include <stdio.h>
#include "fbDev.h"
#include <stdlib.h>
#include "memFile.h"
#include "tickMs.h"

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() ){
         anigif_t *anigif ;
         if( aniGIF( fIn.getData(), fIn.getLength(), anigif ) ){
            printf( "parsed animation: %p %u images %ux%u\n", 
                    anigif,
                    anigif->numImages_,
                    anigif->width_,
                    anigif->height_ );
            unsigned const xPos = ( 2 < argc ) ? strtoul( argv[2], 0, 0 ) : 0 ;
            unsigned const yPos = ( 3 < argc ) ? strtoul( argv[3], 0, 0 ) : 0 ;
            fbDevice_t &fb = getFB();

while( 1 ){
            fb.rect( xPos, yPos, xPos + anigif->width_, anigif->height_, 
                     fb.getRed( anigif->bgColor_ ), 
                     fb.getGreen( anigif->bgColor_ ),
                     fb.getBlue( anigif->bgColor_ ) );

            for( unsigned i = 0 ; i < anigif->numImages_ ; i++ ){
               aniImage_t *img = anigif->images_[i];
               printf( "[%u] == %p\n", i, img );
               printf( "   %u:%u %u:%u\n", img->xLeft_, img->yTop_, img->width_, img->height_ );
               if( img ){
                  printf( "   delay %u\n", img->delay100ths_ );
                  if( img->hasTransparency_ ){
                     printf( "   transparent color %04x\n", img->transparentColor_ );
                     fb.render( xPos + img->xLeft_, yPos+ img->yTop_, img->width_, img->height_, img->data_, 
                                img->transparentColor_,
                                anigif->bgColor_ );
                  }
                  else {
                     printf( "   no transparency\n" );
                     fb.render( xPos + img->xLeft_, yPos+ img->yTop_, img->width_, img->height_, img->data_ );
                  }
                  long long start = tickMs(); 
                  long long diff = img->delay100ths_*10 ;
                  while( tickMs()-start < diff ){
                     ;
                  }
               }
               else
                  printf( "Invalid image\n" );
            }
break ;
}
            anigif->dispose(anigif);
         }
         else
            printf( "Error parsing animation %s\n", argv[1] );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: imgFile fileName [x [y]]\n" );
   return 0 ;
}   
#endif
