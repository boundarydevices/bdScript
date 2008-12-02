/*
 * Module rotateBlt.cpp
 *
 * This module defines the bit-blt routines declared
 * in rotateBlt.h
 *
 * Change History : 
 *
 * $Log: rotateBlt.cpp,v $
 * Revision 1.2  2008-12-02 00:23:15  ericn
 * prevent compiler warning signed/unsigned
 *
 * Revision 1.1  2008-11-03 16:40:43  ericn
 * Added rotateBlt module
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "rotateBlt.h"
#include <string.h>

// #define DEBUGPRINT
#include "debugPrint.h"
#include <stdio.h>

void bltNormalized( rotation_e, 
                    void const *src, unsigned srcStride,
                    void       *dest, int perPixelAdder, int perLineAdder,
                    unsigned    bytesPerPixel,       // copy this many bytes for each pixel
                    unsigned    numPixels,           // and this many pixels per line
                    unsigned    numLines )           // and this many lines
{
   debugPrint( "    %p[%u]->%p[%d..%d] %u pixels of %u bytes, %u lines\n", 
                src, srcStride, 
                        dest, perPixelAdder, perLineAdder, 
                                   numPixels, bytesPerPixel, numLines );
   for( unsigned y = 0 ; y < numLines ; y++ ){
      char *nextIn = (char *)src ;
      src = (char *)src + srcStride ;
      char *nextOut = (char *)dest ;
      for( unsigned x = 0 ; x < numPixels ; x++ ){
         memcpy( nextOut, nextIn, bytesPerPixel );
         nextIn += bytesPerPixel ;
         nextOut += perPixelAdder ;
      }
      dest = (char *)dest + perLineAdder ;
   }
}

void rotateBlt( rotation_e  rotation,
                unsigned    bytesPerPixel,
                unsigned    inputWidth,
                unsigned    inputHeight,
                void const *src, int srcX, int srcY, unsigned srcW, unsigned srcH,
                void       *dst, int dstX, int dstY, unsigned dstW, unsigned dstH )
{
   int w = inputWidth ;  // move to signed integer field
   int h = inputHeight ; //         "

printf( "rotation %d, bpp %u, %ux%u\n"
	"src %u:%u (%ux%u) -> %p\n"
	"dst %u:%u (%ux%u) -> %p\n",
	rotation, bytesPerPixel, inputWidth, inputHeight,
	srcX, srcY, srcW, srcH, src,
	dstX, dstY, dstW, dstH, dst );
   //
   // adjust zeros (source affects dest)
   //
   if( 0 > srcX ){
      w    += srcX ; // subtract
      dstX -= srcX ; // add
      srcX = 0 ;
   }
   if( 0 > srcY ){
      h    += srcY ; // subtract
      dstY -= srcY ; // subtract
      srcY = 0 ;
   }
   if( 0 > dstX ){
      w += dstX ; // subtract
      dstX = 0 ;
   }
   if( 0 > dstY ){
      h += dstY ; // subtract
      dstY = 0 ;
   }

   if( ((unsigned)srcX >= srcW)
     ||((unsigned)srcY >= srcH)
     ||((unsigned)dstX >= dstW)
     ||((unsigned)dstY >= dstH)
     ||(0 >= w)
     ||(0 >= h))
      return ;

   //
   // clamp right-hand edges
   //
   if( (unsigned)srcX+w > srcW )
      w = srcW-srcX ;
   if( (unsigned)srcY+h > srcH )
      h = srcH-srcY ;
   if( (unsigned)dstX+w > dstW )
      w = dstW-dstX ;
   if( (unsigned)dstY+h > dstH )
      h = dstH-dstY ;
   
   if((0==w)||(0==h))
      return ;

   //
   // normalize pointers
   //

   // easy one first
   unsigned srcStride = bytesPerPixel*srcW ;
   src = (char *)src + srcY*srcStride + srcX*bytesPerPixel ;

   // destination is harder...

   int perPixelAdder = 0 ;
   int perLineAdder = 0 ;

   // bail out for the rest...
   switch( rotation ){
      case ROTATE90: {
         unsigned physLine = dstH*bytesPerPixel ;
         perPixelAdder = 0-physLine ;
         perLineAdder  = bytesPerPixel ;
         dst = (char *)dst + (dstW-dstX-1)*physLine ;
         dst = (char *)dst + dstY*perLineAdder ;
//         w ^= h ;
//         h ^= w ;
         break;
      }
      case ROTATE180: {
         unsigned physLine = dstW*bytesPerPixel ;
         perPixelAdder = 0-bytesPerPixel ;
         perLineAdder  = 0-physLine ;
         dst = (char *)dst + (dstH-dstY-1)*physLine ;
         dst = (char *)dst + dstX*perPixelAdder ;
         break;
      }
      case ROTATE270: {
         unsigned physLine = dstH*bytesPerPixel ;
         perPixelAdder = physLine ;
         perLineAdder  = 0-bytesPerPixel ;
         dst = (char *)dst + (dstH-dstY-1)*bytesPerPixel ;
         dst = (char *)dst + dstX*physLine ;
         break;
      }
      default: // assume ROTATE0
         perPixelAdder = bytesPerPixel ;
         perLineAdder = bytesPerPixel*dstW ;
         dst = (char *)dst + dstY*perLineAdder + dstX*perPixelAdder ;
   }

   bltNormalized( rotation, 
                  src, srcStride,
                  dst, perPixelAdder, perLineAdder,
                  bytesPerPixel, w, h );
}

#ifdef MODULETEST_ROTATEBLT

#include <stdio.h>
#include "fbDev.h"
#include "imgFile.h"
#include <stdlib.h>
#include <ctype.h>

static int rotation = 0 ;
static int destx = 0 ;
static int desty = 0 ;
static int destw = 0 ;
static int desth = 0 ;

static int srcx = 0 ;
static int srcy = 0 ;

static void parseArgs( int &argc, char const **argv )
{
	for( unsigned arg = 1 ; arg < argc ; arg++ ){
		if( '-' == *argv[arg] ){
			char const *param = argv[arg]+1 ;
			if( 'r' == tolower(*param) ){
            			rotation = strtoul(param+1,0,0)&ROTATE270;
			}
			else if( 'x' == tolower(*param) ){
            			destx = strtoul(param+1,0,0);
			}
			else if( 'y' == tolower(*param) ){
            			desty = strtoul(param+1,0,0);
			}
			else if( 'w' == tolower(*param) ){
            			destw = strtoul(param+1,0,0);
			}
			else if( 'h' == tolower(*param) ){
            			desth = strtoul(param+1,0,0);
			}
			else if( 's' == tolower(*param) ){
                           if( 'x' == tolower(param[1]) ){
            			srcx = strtoul(param+2,0,0);
                           }
                           else if( 'y' == tolower(param[1])){
            			srcy = strtoul(param+2,0,0);
                           }
                           else 
                              printf( "Usage: -sxNNN or -syNNN\n" );
			}
			else
				printf( "unknown option %s\n", param );

			// pull from argument list
			for( int j = arg+1 ; j < argc ; j++ ){
				argv[j-1] = argv[j];
			}
			--arg ;
			--argc ;
		}
                else
                   printf( "not flag: %s\n", argv[arg] );
	}
}

int main( int argc, char const **argv )
{
   printf( "Hello, %s\n", argv[0] );
   parseArgs(argc,argv);
   if( 2 <= argc ){
      printf( "rotation == %d\n", rotation );
      fbDevice_t &fb = getFB();

      unsigned char *fbMem = (unsigned char *)fb.getMem();
      printf( "physMem %p\n", fbMem );
      for( int arg = 1 ; arg < argc ; arg++ ){
         image_t img ;
         if( imageFromFile( argv[arg], img ) ){
            printf( "%s: %p %ux%u\n", argv[arg], img.pixData_, img.width_, img.height_ );
            unsigned virtX = (0 == (rotation&1)) ? fb.getWidth() : fb.getHeight();
            unsigned virtY = (0 == (rotation&1)) ? fb.getHeight() : fb.getWidth();
            unsigned w = destw ;
            unsigned h = desth ;
            if( 0 == w )
               w = img.width_ ;
            if( 0 == h )
               h = img.height_ ;
            rotateBlt( (rotation_e)rotation, 
                       2,
                       w, h,
                       img.pixData_, srcx, srcy, img.width_, img.height_,
                       fbMem, destx, desty, virtX, virtY );
         }
         else
            perror( argv[arg] );
      }
   }
   else
      fprintf( stderr, "Usage: %s imgFile [imgFile...]\n", argv[0] );

   return 0 ;
}

#endif
