/*
 * Module yuvImgFile.cpp
 *
 * This module defines the yuvImageFromFile() routine
 * as declared in yuvImgFile.h
 *
 *
 * Change History : 
 *
 * $Log: yuvImgFile.cpp,v $
 * Revision 1.1  2008-06-08 19:52:13  ericn
 * add jpeg->YUV support
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "yuvImgFile.h"
#include "memFile.h"
#include "jpegToYUV.h"
#include <string.h>

// #define DEBUGPRINT
#include "debugPrint.h"

#define __USEJPEG__ 1

bool yuvImageFromFile( char const *fileName,
                       image_t    &image )
{
   image.unload();
   memFile_t fIn( fileName );
   if( fIn.worked() )
   {
      bool            worked = false ;
      void const     *pixMap = 0 ;
      void const     *alpha = 0 ;
      unsigned short  width ;
      unsigned short  height ;
      char const *cData = (char const *)fIn.getData();

#ifdef __USEJPEG__

      if( ('\xff' == cData[0] ) && ( '\xd8' == cData[1] ) ) // && ( '\xff' == cData[2] ) && ( '\xe0' == cData[3] ) )
      {
         worked = jpegToYUV( cData, fIn.getLength(), pixMap, width, height );
      }
      else 
#endif
      {
         debugPrint( "%s: Unknown file type %02x %02x %02x %02x\n", fileName, 
                     cData[0], cData[1], cData[2], cData[3] );
         // empty else clause
      }

      if( worked )
      {
         image.pixData_ = pixMap ;
         image.width_   = width ;
         image.height_  = height ;
         image.alpha_   = alpha ;
         return true ;
      }
   }

   return false ;

}

#ifdef STANDALONE
#include <stdio.h>
#ifdef __DAVINCIFB__
#include "fbDevices.h"
#else
#include "fbDev.h"
#endif
#include <stdlib.h>
#include "yuvSignal.h"
#include <linux/sm501yuv.h>
#include <linux/sm501-int.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      image_t image ;
      if( yuvImageFromFile( argv[1], image ) )
      {
         printf( "Image read: %ux%u pixels\n", image.width_, image.height_ );

         unsigned x = 0 ;
         if( 3 <= argc )
            x = strtoul( argv[2], 0, 0 );

         unsigned y = 0 ;
         if( 4 <= argc )
            y = strtoul( argv[3], 0, 0 );
         yuvSignal_t &yuv = yuvSignal_t::get();
         if( yuv.isOpen() ){
            fbDevice_t &fb = getFB();
            struct sm501yuvPlane_t plane ;
            plane.xLeft_     = x ;
            plane.yTop_      = y ;
            plane.inWidth_   = image.width_ ;
            plane.inHeight_  = image.height_ ;
            plane.outWidth_  = fb.getWidth()-x ;
            plane.outHeight_ = fb.getHeight()-y ;
printf( "display at %u:%u (%ux%u), input %ux%u\n", plane.xLeft_, plane.yTop_, plane.outWidth_, plane.outHeight_, plane.inWidth_, plane.inHeight_ );
            if( 0 != ioctl( yuv.getFd(), SM501YUV_SETPLANE, &plane ) )
            {
               perror( "setPlane" );
printf( "output display at %u:%u (%ux%u), input %ux%u\n", plane.xLeft_, plane.yTop_, plane.outWidth_, plane.outHeight_, plane.inWidth_, plane.inHeight_ );
printf( "fd is %d\n", yuv.getFd() );
            }
            else {
               debugPrint( "setPlane success\n"
                           "%u:%u %ux%u -> %ux%u\n"
                           "offset == 0x%x\n", 
                           plane.xLeft_, plane.yTop_,
                           plane.inWidth_, plane.inHeight_,
                           plane.outWidth_, plane.outHeight_,
                           plane.planeOffset_ );
               fbDevice_t &fb = getFB();
               reg_and_value rv ;
               rv.reg_ = SMIVIDEO_CTRL ;
               rv.value_ = SMIVIDEO_CTRL_ENABLE_YUV ;
               int res = ioctl( fb.getFd(), SM501_WRITEREG, &rv );
               if( 0 == res ){
                  unsigned bytes = 2*image.width_*image.height_ ;
                  write( yuv.getFd(), image.pixData_, bytes );
                  debugPrint( "enabled YUV\n" );
               }
               else
                  debugPrint( "Error enabling YUV\n" );
            }
         char inBuf[512];
         fgets(inBuf,sizeof(inBuf),stdin);
         }
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: imgFile fileName [x [y]]\n" );
   return 0 ;
}   
#endif
