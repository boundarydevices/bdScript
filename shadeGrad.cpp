/*
 * Program shadeGrad.cpp
 *
 * This program creates a shadow gradient of the specified length.
 * The gradient will be a set of byte values (range [0..255]) output
 * to /tmp/shadow.dat.
 *
 * The values are specified by using wrapping the length around
 * 1/2 of an imaginary circle, and using the cosine of the angle
 * at each point along the length as an opacity value.
 *
 * In order to allow the shadow to appear sharper or more gradual,
 * a 'power' variable, with a default of 1 may be specified on the 
 * command line. Greater values provide a steeper drop-off from the 
 * center.
 *
 * To allow movement of the shadow, an offset value in pixels 
 * (default 0) may also be specified on the command line.
 *
 * Change History : 
 *
 * $Log: shadeGrad.cpp,v $
 * Revision 1.1  2006-05-07 15:42:23  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include <stdio.h>
#include <signal.h>
#include <math.h>
#include <stdlib.h>

int main( int argc, char const *const argv[] )
{
	if( 2 <= argc )
	{
		unsigned const length = strtoul(argv[1], 0, 0);
		if( 0 < length )
		{
			unsigned range = length ;
			if( 2 < argc ){
				range = strtoul(argv[2], 0, 0);
				if( ( 0 == range ) || (length <range) ){
					fprintf( stderr, "Invalid range: %s\n", argv[2] );
				}
			}

			float power = 1.0 ;
			if( 3 < argc ){
				sscanf( argv[3], "%f", &power );
				if( 0.0 == power ){
					fprintf( stderr, "Invalid power %s\n", argv[3] );
					return -1 ;
				}
					
			}

			float divisor = 1.0 ;
			if( 4 < argc ){
				sscanf( argv[4], "%f", &divisor );
				if( 0.0 == divisor ){
					fprintf( stderr, "Invalid divisor %s\n", argv[4] );
					return -1 ;
				}
					
			}

			int offset = 0 ;
			if( 5 < argc ){
				if( 1 != sscanf( argv[5], "%d", &offset ) ){
					fprintf( stderr, "Invalid offset %s\n", argv[5] );
					return -1 ;
				}
			}
				

			char const *fileName = ( 6 < argc )
						? argv[6]
						: "/tmp/shadow.dat" ;

			FILE *fOut = fopen( fileName, "wb" );
			if( fOut ){
				unsigned min = (offset >= 0)
						? offset
						: 0 ;
				unsigned max = ((offset+range) < length)
						? offset+range
						: length ;
				// imaginary circle
				double radius = (double)range / 2.0 ;
				for( unsigned y = 0 ; y < length ; y++ ){

					unsigned char d ;

					if( (y >= min) && (y < max) ) {
						double ypos = (double)(radius-y+offset) ; // center is zero
						ypos /= radius ;
						double a = asin( ypos );
						double c = cos(a);
						double frac = pow(c,power); // make it pointy-er
						frac /= divisor ;
	
						if( 0.0 > frac )
							frac = 0.0 ; // don't go negative
						if( 1.0 < frac )
							frac = 1.0 ; // clamp max
						d = (unsigned char)floor(255*frac);
					}
					else
						d = 0 ;
					printf( "%u\n", d );
					fwrite( &d, 1, sizeof(d), fOut );
				}
				fclose( fOut );
				
				fprintf( stderr, "%u samples written to %s\n", length, fileName );
			}
			else
				perror( fileName );
		}
		else
			fprintf( stderr, "Invalid length\n" );
	}
	else
		fprintf( stderr, "Usage: %s length [range=length] [power=1] [divisor=1] [offset=0] [fileName=/tmp/shadow.dat]\n", argv[0] );
       
       return 0 ;
}
