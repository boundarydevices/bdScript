/*
 * Module cylinderShadow.cpp
 *
 * This module defines the cylinderShadow()
 * routine as declared in cylinderShadow.h
 *
 * Change History : 
 *
 * $Log: cylinderShadow.cpp,v $
 * Revision 1.1  2006-11-06 10:36:12  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "cylinderShadow.h"
#include <string.h>
#include <math.h>

fbPtr_t cylinderShadow( unsigned width,
                        unsigned height,
                        double   power,           // > 1 deepens the shadow
                        double   divisor,         // > 1 makes it lighter
                        unsigned range,           // < height to fill top/bottom w/black
                        unsigned offset )         // <> 0 to shift
{
   unsigned const numPix = width*height ;
   unsigned const numBytes = numPix*2 ;
   fbPtr_t rval( numBytes );
   memset( rval.getPtr(), 0, numBytes );

   // We'll dither to get better results.
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

   unsigned min = (offset >= 0)  ? offset : 0 ;
   unsigned max = ((offset+range) < height) ? offset+range  : height ;

   // imaginary circle
   double const radius = (double)range / 2.0 ;
   unsigned short *nextOut = (unsigned short *)rval.getPtr();

   for( unsigned y = 0 ; y < height ; y++ )
   {
      unsigned char darkness ;
      if( (y >= min) && (y < max) ) {
         double ypos = (double)(radius-y+offset) ; // center is zero
         ypos /= radius ;
         double angle = asin( ypos );
         double c = cos(angle);
         double frac = pow(c,power); // make it pointy-er
         frac /= divisor ;

         if( 0.0 > frac )
            frac = 0.0 ; // don't go negative
         if( 1.0 < frac )
            frac = 1.0 ; // clamp max
         darkness = (unsigned char)floor(255*frac);
      }
      else {
         darkness = 0 ;
      }

      unsigned char darkOpacity = ~darkness ;
      unsigned char a = darkOpacity ;
      double const desired = 1.0*a ;

      rightError = 0.0 ;

      double *useDown   = downErrors[y&1];
      double *buildDown = downErrors[1^(y&1)];

      buildDown[0] =
      buildDown[1] = 0.0 ;

      for( unsigned x = 0 ; x < width ; x++ ){
         double value = desired + rightError + *useDown++ ;
         unsigned char const quant = (unsigned char)floor((value*15.0)/255.0);
         double actual = (255.0*quant)/15 ;
         double error = value - actual ;
         *nextOut++ = ((unsigned short)quant ) << 12 ;

         rightError = (error*7)/16 ;
         buildDown[0] += (error*3)/16 ;
         if( x < width - 2 ){
            buildDown[1] += (error*5)/16 ;
            buildDown[2]  = (error)/16 ;
         }
         buildDown++ ;
      }
   } // for each row

   return rval ;
}


#ifdef MODULETEST

#include <stdio.h>
#include <stdlib.h>
#include "sm501alpha.h"

int main( int argc, char const * const argv[] )
{
	if( 5 <= argc )
	{
      unsigned const x = strtoul(argv[1], 0, 0);
		unsigned const y = strtoul(argv[2], 0, 0);
      unsigned const width = strtoul(argv[3], 0, 0);
		unsigned const height = strtoul(argv[4], 0, 0);
   	
		if( ( 0 < width ) && ( 0 < height ) )
		{
			unsigned range = height ;
			if( 5 < argc ){
				range = strtoul(argv[5], 0, 0);
				if( ( 0 == range ) || (height <range) ){
					fprintf( stderr, "Invalid range: %s\n", argv[5] );
				}
			}

			float power = 1.0 ;
			if( 6 < argc ){
				sscanf( argv[6], "%f", &power );
				if( 0.0 == power ){
					fprintf( stderr, "Invalid power %s\n", argv[6] );
					return -1 ;
				}
			}

			float divisor = 1.0 ;
			if( 7 < argc ){
				sscanf( argv[7], "%f", &divisor );
				if( 0.0 == divisor ){
					fprintf( stderr, "Invalid divisor %s\n", argv[7] );
					return -1 ;
				}
			}

			int offset = 0 ;
			if( 8 < argc ){
				if( 1 != sscanf( argv[8], "%d", &offset ) ){
					fprintf( stderr, "Invalid offset %s\n", argv[8] );
					return -1 ;
				}
			}

         fbPtr_t shadow = cylinderShadow( width, height, power, divisor, range, offset );
         sm501alpha_t &alphaLayer = sm501alpha_t::get( sm501alpha_t::rgba4444 );
         alphaLayer.draw4444( (unsigned short *)shadow.getPtr(), x, y, width, height );

         char inBuf[80];
         fgets( inBuf, sizeof(inBuf), stdin );
		}
		else
			fprintf( stderr, "Invalid height\n" );
	}
	else
		fprintf( stderr, "Usage: %s x y width height [range=height] [power=1] [divisor=1] [offset=0] [fileName=/tmp/shadow.dat]\n", argv[0] );
       
   return 0 ;

}

#endif
