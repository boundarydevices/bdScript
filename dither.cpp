/*
 * Module dither.cpp
 *
 * This module defines the methods of the dither_t class, 
 * which is used to generate and store the results of the
 * Floyd-Steinberg dither as described in dither.h
 *
 *
 * Change History : 
 *
 * $Log: dither.cpp,v $
 * Revision 1.2  2004-05-08 19:21:02  ericn
 * -fixed rounding, tried to speed it up
 *
 * Revision 1.1  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "dither.h"
#include "fbDev.h"
#include <string.h>
#include <stdio.h>

static unsigned char const downDithers[] = {
   3,5,1
};

static unsigned char const rightDither = 7 ;

#define MAX( _first, _second ) ( ((_first) > (_second))?(_first):(_second) )
#define MIN( _first, _second ) ( ((_first) < (_second))?(_first):(_second) )

static int luminance( int red, int green, int blue )
{
   // 
   // I've seen a couple of different algorithms here:
   // (max+min)/2
/*

   int max = MAX( red, MAX( green, blue ) );
   int min = MIN( red, MIN( green, blue ) );
   return (max+min)/2 ;
*/

   // A more mathematically-correct version
// return (int)(c.R*0.3 + c.G*0.59+ c.B*0.11);

   // Just return 'green'
   // return green;

   // and one that uses shifts and adds to come close to the above
   //
   //    red   = 5/16= 0.3125    == 1/4 + 1/16
   //    green = 9/16= 0.5625    == 1/2 + 1/16
   //    blue  = 1/8 = 0.125
   //
   if( 0 < red )
      red = red>>2 + red>>4 ;
   else
      red = 0 ;
   if( 0 < green )
      green = green>>1 + green>>4 ;
   else
      green = 0 ;
   if( 0 < blue )
      blue = blue >> 3 ;
   else
      blue = 0 ;
   return red+green+blue ;
}


#define REDERROR(x)    ((x)*3)
#define GREENERROR(x)  (((x)*3)+1)
#define BLUEERROR(x)   (((x)*3)+2)


dither_t :: dither_t
   ( unsigned short const rgb16[],
     unsigned width, unsigned height )
   : bits_( new unsigned char [ ((width*height)+7)/8 ] )
   , width_( width )
   , height_( height )
{
   // 
   // need 1 forward error value and three downward errors for 
   // Floyd-Steinberg dither. Allocate two rows for down-errors,
   // since we'll be building one and using the other
   //
   // 	    X      7/16
   //       3/16   5/16  1/16    
   //

   unsigned const downErrorMax = 3*width ;
   int *const downErrors[2] = {
      new int[downErrorMax],
      new int[downErrorMax]
   };

   memset( downErrors[0], 0, downErrorMax * sizeof( downErrors[0][0] ) );
   memset( downErrors[1], 0, downErrorMax * sizeof( downErrors[1][0] ) );

   int rightErrors[3];
   
   fbDevice_t &fb = getFB();

   unsigned bitOffset = 0 ;
   unsigned char *const outBits = (unsigned char *)bits_ ;

   for( int y = 0 ; y < height ; y++ )
   {
      // not carrying errors from right-edge to left
      memset( rightErrors, 0, sizeof( rightErrors ) );

      int * const useDown   = downErrors[ y & 1];
      int * const buildDown = downErrors[ (~y) & 1 ];
//      memset( buildDown, 0, downErrorMax * sizeof( downErrors[1][0] ) );

      int errPos ;
      for( int x = 0, errPos = 0 ; x < width ; x++, bitOffset++, errPos += 3 )
      {
         unsigned short inPix = rgb16[y*width+x];

         int const blue = ( (inPix & 0x1f) << 3 );
         inPix >>= 5 ;
         int const green = ( (inPix & (0x3f<<5)) >> (5-2) );
         inPix >>= 6 ;
         int const red = ( inPix << 3 );

         int colors[3] = { red, green, blue };
/* 200 ms in this line */
//         int colors[3] = { fb.getRed( inPix ), fb.getGreen( inPix ), fb.getBlue( inPix ) };

/* 300 ms in this loop */
         for( unsigned c = 0 ; c < 3 ; c++ )
            colors[c] += useDown[errPos+c] + rightErrors[c];

/* 100 ms in luminance */
//         int const l = luminance( colors[0], colors[1], colors[2] );
         int const l = colors[1];
         int actual[3];
         int actualRed, actualGreen, actualBlue ;
         unsigned char const mask = ( 1 << (bitOffset&7) );
         unsigned const outByte = bitOffset / 8 ;

         if( l > 0x80 )
         {
            outBits[outByte] |= mask ;
            actual[0] = 0xF8 ;
            actual[1] = 0xFC ;
            actual[2] = 0xF8 ;
         } // output white
         else
         {
            outBits[outByte] &= ~mask ;
            actual[0] = actual[1] = actual[2] = 0 ;
         } // output black

/* 300 ms below */

         //
         // now calculate and store errors
         //
         int buildStart = errPos ;
         for( unsigned c = 0 ; c < 3 ; c++ )
         {
            int const diff = colors[c] - actual[c];
            int const sixteenths = diff/16 ;
            
            rightErrors[c] = 7*sixteenths ;

            if( 0 < x )
            {
               buildDown[buildStart-3+c] += 3*sixteenths ;
               buildDown[buildStart+c] += 5*sixteenths ;
            }
            else
               buildDown[buildStart+c] = 5*sixteenths ;

            if( x < width - 1 )
               buildDown[buildStart+3+c] = sixteenths ;
         }

/*
if( ( 2 >= y ) && ( 5 > x ) )
{
   printf( "---------> x %d, y %d\n", x, y );
   printf( "desired %d/%d/%d\n", red, green, blue );
   printf( "actual  %d/%d/%d\n", actualRed, actualGreen, actualBlue );
   printf( "errors  %d/%d/%d\n", rightErrors[0], rightErrors[1], rightErrors[2] );
}
*/
      }
   }

   delete [] downErrors[0];
   delete [] downErrors[1];
}

