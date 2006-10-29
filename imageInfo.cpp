/*
 * Module imageInfo.cpp
 *
 * This module defines the getImageInfo() routine
 * as declared in imageInfo.h
 *
 *
 * Change History : 
 *
 * $Log: imageInfo.cpp,v $
 * Revision 1.1  2006-10-29 21:59:10  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "imageInfo.h"
#include <string.h>
#include <netinet/in.h>

//#define DEBUGPRINT
#include "debugPrint.h"

#define __USEJPEG__ 1
#define __USEPNG__ 1
#define __USEGIF__ 1

#ifdef __USEPNG__

static char const pngHeader[] = {
   137 
,  80 
,  78 
,  71 
,  13 
,  10 
,  26 
,  10 
};

unsigned const pngHeaderLen = 8 ;
unsigned const pngChunkOverhead = 12 ;

#define CHUNKID( __str )   *((unsigned long const *)__str)

static unsigned long const ihdr_chunk = CHUNKID( "IHDR" );

struct ihdr_struct {
   unsigned long width ;
   unsigned long height ;
   unsigned char depth ;
   unsigned char color_type ;
   unsigned char compression ;
   unsigned char filter ;
   unsigned char interlace ;
} __attribute__ ((packed));

//
// find chunk: See http://libpng.org/pub/png/spec/1.1/
//
static bool findChunk( void const     *imgData,
                       unsigned        imgLen,
                       unsigned long   chunkId,
                       void const    *&chunkData,
                       unsigned       &chunkLen )
{
   if( 0 == memcmp( imgData, pngHeader, pngHeaderLen ) ){
      unsigned char const * const end = (unsigned char const *)imgData + imgLen - pngChunkOverhead ;
      unsigned char const *nextChunk = (unsigned char const *)imgData + pngHeaderLen ;
      while( nextChunk < end ){
         unsigned long thisChunkLen ;
         memcpy( &thisChunkLen, nextChunk, sizeof(chunkLen) );
         thisChunkLen = ntohl(thisChunkLen);
         unsigned long thisChunkId ;
         memcpy( &thisChunkId, nextChunk+sizeof(chunkLen), sizeof(thisChunkId) );
debugPrint( "chunkId: %08lx, len %lu\n", thisChunkId, thisChunkLen );
         if( thisChunkId == chunkId ){
            chunkData = nextChunk+sizeof(chunkLen)+sizeof(thisChunkId);
            chunkLen  = thisChunkLen ;
            return chunkLen < (unsigned)( end-(unsigned char *)nextChunk );
         }
         nextChunk += thisChunkLen + pngChunkOverhead ;
      } // walk each chunk
   } // valid header

   return false ;
}

static bool getPngDim( void const  *imgData,        // input
                       unsigned     imgLen,         // input
                       imageInfo_t &details )       // output
{
   void const *chunkData ;
   unsigned    chunkLen ;
   if( findChunk( imgData, imgLen, ihdr_chunk, chunkData, chunkLen ) 
       &&
       ( sizeof(ihdr_struct) == chunkLen ) )
   {
      ihdr_struct const &ihdr = *(ihdr_struct const *)chunkData ;
      details.width_ = ntohl(ihdr.width);
      details.height_ = ntohl(ihdr.height);

      debugPrint( "found header: %u/%u\n", details.width_, details.height_ );
      return true ;
   }
   else
      debugPrint( "no chunk or invalid size\n" );

   return false ;
}
#endif

#ifdef __USEJPEG__

typedef enum {          /* JPEG marker codes                    */
  M_SOF0  = 0xc0,       /* baseline DCT                         */
  M_SOF1  = 0xc1,       /* extended sequential DCT              */
  M_SOF2  = 0xc2,       /* progressive DCT                      */
  M_SOF3  = 0xc3,       /* lossless (sequential)                */
  
  M_SOF5  = 0xc5,       /* differential sequential DCT          */
  M_SOF6  = 0xc6,       /* differential progressive DCT         */
  M_SOF7  = 0xc7,       /* differential lossless                */
  
  M_JPG   = 0xc8,       /* JPEG extensions                      */
  M_SOF9  = 0xc9,       /* extended sequential DCT              */
  M_SOF10 = 0xca,       /* progressive DCT                      */
  M_SOF11 = 0xcb,       /* lossless (sequential)                */
  
  M_SOF13 = 0xcd,       /* differential sequential DCT          */
  M_SOF14 = 0xce,       /* differential progressive DCT         */
  M_SOF15 = 0xcf,       /* differential lossless                */
  
  M_DHT   = 0xc4,       /* define Huffman tables                */
  
  M_DAC   = 0xcc,       /* define arithmetic conditioning table */
  
  M_RST0  = 0xd0,       /* restart                              */
  M_RST1  = 0xd1,       /* restart                              */
  M_RST2  = 0xd2,       /* restart                              */
  M_RST3  = 0xd3,       /* restart                              */
  M_RST4  = 0xd4,       /* restart                              */
  M_RST5  = 0xd5,       /* restart                              */
  M_RST6  = 0xd6,       /* restart                              */
  M_RST7  = 0xd7,       /* restart                              */
  
  M_SOI   = 0xd8,       /* start of image                       */
  M_EOI   = 0xd9,       /* end of image                         */
  M_SOS   = 0xda,       /* start of scan                        */
  M_DQT   = 0xdb,       /* define quantization tables           */
  M_DNL   = 0xdc,       /* define number of lines               */
  M_DRI   = 0xdd,       /* define restart interval              */
  M_DHP   = 0xde,       /* define hierarchical progression      */
  M_EXP   = 0xdf,       /* expand reference image(s)            */
  
  M_APP0  = 0xe0,       /* application marker, used for JFIF    */
  M_APP1  = 0xe1,       /* application marker                   */
  M_APP2  = 0xe2,       /* application marker                   */
  M_APP3  = 0xe3,       /* application marker                   */
  M_APP4  = 0xe4,       /* application marker                   */
  M_APP5  = 0xe5,       /* application marker                   */
  M_APP6  = 0xe6,       /* application marker                   */
  M_APP7  = 0xe7,       /* application marker                   */
  M_APP8  = 0xe8,       /* application marker                   */
  M_APP9  = 0xe9,       /* application marker                   */
  M_APP10 = 0xea,       /* application marker                   */
  M_APP11 = 0xeb,       /* application marker                   */
  M_APP12 = 0xec,       /* application marker                   */
  M_APP13 = 0xed,       /* application marker                   */
  M_APP14 = 0xee,       /* application marker, used by Adobe    */
  M_APP15 = 0xef,       /* application marker                   */
  
  M_JPG0  = 0xf0,       /* reserved for JPEG extensions         */
  M_JPG13 = 0xfd,       /* reserved for JPEG extensions         */
  M_COM   = 0xfe,       /* comment                              */
  
  M_TEM   = 0x01,       /* temporary use                        */

  M_ERROR = 0x100       /* dummy marker, internal use only      */
} JPEG_MARKER;


static int next_marker( char const *&nextIn,
                        char const  *end )
{
  char c ;
  while( nextIn < end ){
     c = *nextIn++ ;
     if( '\xff' == c )
        break ;
  }

  if( nextIn >= end )
     goto out ;

  // skip repeated FF's
  while( nextIn < end ){
     c = *nextIn++ ;
     if( '\xff' != c ){
        return (unsigned char)c ;
     }
  }

out:
  return M_ERROR ;
}


static bool getJpegDim( void const  *imgData,        // input
                        unsigned     imgLen,         // input
                        imageInfo_t &details )       // output
{
   char const *nextIn = (char const *)imgData ;
   char const * const end = nextIn + imgLen ;

   int marker ;
   if( M_SOI == ( marker = next_marker(nextIn,end) ) )
   {
      while( nextIn < end )
      {
         marker = next_marker(nextIn,end);
         switch (marker) {
            case M_ERROR:
               nextIn = end ;
               break ;
   
            /* The following are not officially supported in PostScript level 2 */
            case M_SOF2:
            case M_SOF3:
            case M_SOF5:
            case M_SOF6:
            case M_SOF7:
            case M_SOF9:
            case M_SOF10:
            case M_SOF11:
            case M_SOF13:
            case M_SOF14:
            case M_SOF15:
               debugPrint( "Warning: JPEG file uses compression method %X - proceeding anyway.\n", marker );
               debugPrint( "PostScript output does not work on all PS interpreters!\n");

            case M_SOF0:
            case M_SOF1: {
               // format is 2-byte length, 
               //           1-byte bits per component, 
               //           2-byte height
               //           2-byte width
               //           1-byte number of components
               if( end-nextIn < 8 )
                  return false ;    // not enough space

               unsigned short width, height ;
               memcpy( &height, nextIn+3, sizeof(height) );
               memcpy( &width, nextIn+5, sizeof(width) );
               
               details.width_  = ntohs(width);
               details.height_ = ntohs(height);
               return true ;
            }

            default: {
               unsigned short length ;
               memcpy( &length, nextIn, sizeof(length) );
               length = ntohs(length);
               nextIn += length ;
            }
         }
      }
   }
   else {
      debugPrint( "Unknown start marker: %d\n", marker );
   }

   return false ;
}
#endif

#ifdef __USEGIF__
static bool getGifDim( void const  *imgData,        // input
                       unsigned     imgLen,         // input
                       imageInfo_t &details )       // output
{
   return false ;
}
#endif

bool getImageInfo( void const  *imgData,        // input
                   unsigned     imgLen,         // input
                   imageInfo_t &details )       // output
{
   char const *cData = (char const *)imgData ;
   if( 4 < imgLen ){
      bool worked = false ;

#ifdef __USEPNG__
      if( ( 'P' == cData[1] ) && ( 'N' == cData[2] ) && ( 'G' == cData[3] ) )
      {
         details.type_ = image_t::imgPNG ;
         worked = getPngDim( imgData, imgLen, details );
      }
      else 
#endif
#ifdef __USEJPEG__
      
      if( ('\xff' == cData[0] ) && ( '\xd8' == cData[1] ) && ( '\xff' == cData[2] ) && ( '\xe0' == cData[3] ) )
      {
         details.type_ = image_t::imgJPEG ;
         worked = getJpegDim( imgData, imgLen, details );
      }
      else 
#endif
#ifdef __USEGIF__
      if( 0 == memcmp( cData, "GIF8", 4 ) )
      {
         details.type_ = image_t::imgGIF ;
         worked = getGifDim( imgData, imgLen, details );
      }
      else
#endif
         worked = false ;

      return worked ;
   } // long enough for header check

   return false ;
}


#ifdef MODULETEST

#include "memFile.h"
#include <stdio.h>

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc ){
      memFile_t fIn( argv[1] );

      if( fIn.worked() ){
         imageInfo_t info ;
         if( getImageInfo( fIn.getData(), fIn.getLength(), info ) )
         {
            printf( "%ux%u, type %s\n", info.width_, info.height_, image_t::typeName(info.type_) );
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
