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
 * Revision 1.1  2004-03-17 04:56:19  ericn
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

static int luminance( int const colors[3] )
{
   int max = MAX( colors[0], MAX( colors[1], colors[2] ) );
   int min = MIN( colors[0], MIN( colors[1], colors[2] ) );
   return (max+min)/2 ;
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

      for( int x = 0 ; x < width ; x++, bitOffset++ )
      {
         unsigned short inPix = rgb16[y*width+x];
         int colors[3] = { fb.getRed( inPix ), fb.getGreen( inPix ), fb.getBlue( inPix ) };
         for( unsigned c = 0 ; c < 3 ; c++ )
         {
            colors[c] += useDown[(x*3)+c] + rightErrors[c];
         }

         int const l = luminance( colors );

         int actual[3];
         int actualRed, actualGreen, actualBlue ;
         unsigned char const mask = ( 1 << (bitOffset&7) );
         unsigned const outByte = bitOffset / 8 ;

         if( l > 0x80 )
         {
            outBits[outByte] |= mask ;
            actual[0] = actual[1] = actual[2] = 0xFF ;
         } // output white
         else
         {
            outBits[outByte] &= ~mask ;
            actual[0] = actual[1] = actual[2] = 0 ;
         } // output black

         //
         // now calculate and store errors
         //
         int buildStart = 3*x ;
         for( unsigned c = 0 ; c < 3 ; c++ )
         {
            int const diff = colors[c] - actual[c];
            int const sixteenths = diff/16 ;
            
            rightErrors[c] = 7*sixteenths ;

            if( 0 < x )
               buildDown[buildStart-3+c] = 3*sixteenths ;
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

