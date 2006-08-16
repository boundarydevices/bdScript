/*
 * Module odomHighlight.cpp
 *
 * This module defines the createHighlight() routine
 * as declared in odomHighlight.h
 *
 *
 * Change History : 
 *
 * $Log: odomHighlight.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomHighlight.h"
#include <string.h>
#include <math.h>
#include "debugPrint.h"

void createHighlight( unsigned char const *darkGrad,
                      unsigned char const *lightGrad,
                      unsigned             height,
                      unsigned             width,
                      unsigned char       *outData )  // generally fb ram
{
   // 
   // need 1 forward error value and three downward errors for 
   // Floyd-Steinberg dither. Allocate two rows for down-errors,
   // since we'll be building one and using the other
   //
   // 	    X     7/16
   //       3/16   5/16  1/16    
   //       1/16   1/16
   //

   unsigned const downErrorMax = width ;
   double *const downErrors[2] = {
      new double[downErrorMax],
      new double[downErrorMax]
   };

   memset( downErrors[0], 0, downErrorMax*sizeof(double));
   memset( downErrors[1], 0, downErrorMax*sizeof(double));
   double rightError = 0.0 ;

   unsigned char *alpha = outData ;

   unsigned char const *shadow    = darkGrad ;
   unsigned char const *highlight = lightGrad ;

   double maxError = 0.0 ;
   unsigned prevColor = 2 ;

   for( unsigned y = 0 ; y < height ; y++ )
   {
      unsigned char dark = ~shadow[y];
      unsigned char light = highlight[y];
      debugPrint( "%u: %02x/%02x: ", y, dark, light );
      unsigned char color ;
      unsigned char a ;
      if( dark > light ){
         a = dark-light ;
         color = 0 ;
      }
      else {
         a = light-dark ;
         color = 15 ;
      }

      debugPrint( "%02x/%d\n", a, color );

      double const desired = 1.0*a ;

      rightError = 0.0 ;

      unsigned char *nextOut = alpha + (y*width);
      double *useDown   = downErrors[y&1];
      double *buildDown = downErrors[1^(y&1)];

      if( prevColor != color ){
         memset( useDown, 0, downErrorMax*sizeof(double));
      }

      buildDown[0] =
      buildDown[1] = 0.0 ;

      for( unsigned x = 0 ; x < width ; x++ ){
debugPrint( "%f+%f+%f == ", desired, rightError, *useDown );
         double value = desired + rightError + *useDown++ ;
debugPrint( "%f", value );
         unsigned char const quant = (unsigned char)floor((value*15.0)/255.0);
debugPrint( "== 0x%02X", quant );
         double actual = (255.0*quant)/15 ;
         double error = value - actual ;
debugPrint( ", err %f", error );
         double absErr = fabs(error);
         if( absErr > maxError )
            maxError = absErr ;
         unsigned char const mask = ( quant << 4 ) | color ;
         *nextOut++ = mask ;

         rightError = (error*7)/16 ;
         buildDown[0] += (error*3)/16 ;
         if( x < width - 2 ){
            buildDown[1] += (error*5)/16 ;
            buildDown[2]  = (error)/16 ;
         }
         buildDown++ ;
debugPrint( "\n" );
      }

      debugPrint( "max error: %f\n", maxError );
   }
}

