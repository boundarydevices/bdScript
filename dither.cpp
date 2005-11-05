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
 * Revision 1.6  2005-11-05 20:22:42  ericn
 * -fix compiler warnings
 *
 * Revision 1.5  2005/07/28 21:42:19  tkisky
 * -fix border around dithered image
 *
 * Revision 1.4  2005/07/28 19:16:33  tkisky
 * -try fixing border on image
 *
 * Revision 1.3  2005/01/01 18:28:04  ericn
 * -fixed luminance
 *
 * Revision 1.2  2004/05/08 19:21:02  ericn
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
   red = (red>0) ? ((red + (red<<2))>>4) : 0;
   green = (green>0) ? ((green + (green<<3))>>4) : 0;
   blue = (blue>0) ? (blue>>3) : 0;
   return red+green+blue ;
}


#define REDERROR(x)    ((x)*3)
#define GREENERROR(x)  (((x)*3)+1)
#define BLUEERROR(x)   (((x)*3)+2)

const int maxVals[] = {0xf8,0xfc,0xf8};
const int minVals[] = {0,0,0};

dither_t :: dither_t ( unsigned short const rgb16[], unsigned width, unsigned height )
   :  bits_(new unsigned char [ ((width*height)+31)/8 ]), width_( width ), height_( height )
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
   
//   fbDevice_t &fb = getFB();

   unsigned int mask = 1;
   unsigned int outVal=0;
   unsigned int *outBits = (unsigned int *)bits_ ;

   for( unsigned y = 0 ; y < height ; y++ )
   {
      // not carrying errors from right-edge to left
      memset( rightErrors, 0, sizeof( rightErrors ) );

      int * useDown   = downErrors[ y & 1];
      int * buildDown = downErrors[ (~y) & 1 ];
//      memset( buildDown, 0, downErrorMax * sizeof( downErrors[1][0] ) );

      const unsigned short * pCur = &rgb16[y*width];
      for( unsigned x = 0; x < width ; x++ )
      {
         unsigned short inPix = *pCur++;
         int colors[3];
         colors[0] = (inPix>>(5+6-3)) & 0xf8;   //red
         colors[1] = (inPix>>(5-2)) & 0xfc;     //green
         colors[2] = (inPix<<3) & 0xf8;         //blue
/* 200 ms in this line */
//         int colors[3] = { fb.getRed( inPix ), fb.getGreen( inPix ), fb.getBlue( inPix ) };

/* 300 ms in this loop */
         for( unsigned c = 0 ; c < 3 ; c++ )
            colors[c] += *useDown++ + rightErrors[c];

/* 100 ms in luminance */
         int const l = luminance( colors[0], colors[1], colors[2] );
//         int const l = colors[1];

         const int *actual;
         if (l > 0x80) {
            outVal |= mask ;  // output white
            actual = maxVals;
         } else {
            actual = minVals;	// output black
         } 
         mask <<= 1;
         if (mask==0) {mask=1; *outBits++ = outVal; outVal=0;}

/* 300 ms below */
         //
         // now calculate and store errors
         //
         for( unsigned c = 0 ; c < 3 ; c++ )
         {
            int const diff = colors[c] - actual[c];
            rightErrors[c] = (7*diff)>>4 ;
            if (x > 0) {
               buildDown[-3] += (3*diff)>>4 ;
               *buildDown++ += (5*diff)>>4 ;
            } else {
               *buildDown++ = (5*diff)>>4 ;
            }

            if (x < (width-1)) buildDown[3-1] = diff>>4 ;
         }

/*
if( ( 2 >= y ) && ( 5 > x ) )
{
   printf( "---------> x %d, y %d\n", x, y );
   printf( "desired %d/%d/%d\n", red, green, blue );
   printf( "errors  %d/%d/%d\n", rightErrors[0], rightErrors[1], rightErrors[2] );
}
*/
      }
   }
   if (mask!=1) *outBits = outVal;

   delete [] downErrors[0];
   delete [] downErrors[1];
}

