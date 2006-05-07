/*
 * Program digitStrip.cpp
 *
 * This program creates a 'digit strip' for use with an
 * odometer by using the Free-Type wrapper classes and 
 * the imgToPNG() routine
 *
 *
 * Change History : 
 *
 * $Log: digitStrip.cpp,v $
 * Revision 1.1  2006-05-07 15:41:21  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include <stdio.h>
#include "fbDev.h"
#include "imgFile.h"
#include "ftObjs.h"
#include "memFile.h"
#include <signal.h>
#include <math.h>
#include <time.h>
#include "imgToPNG.h"

struct point_t {
	unsigned x ;
	unsigned y ;
};

unsigned const decimalPointWidth = 18 ; // width of text area
unsigned const decimalPointBlack = 10 ; // how much black (fg)


unsigned const dollarPointSize = 70 ;
unsigned const leftShadowWidth    = 1 ;
unsigned const leftHighlightWidth = 2 ;
// digit text goes here
unsigned const rightShadowWidth   = 2 ;
unsigned const barHeight 	  = 112 ;
unsigned       topMargin 	  = 0 ;
unsigned       bottomMargin	  = 0 ;
	
inline void rgbParts( unsigned long rgb,
		      unsigned	   &r,
		      unsigned     &g,
		      unsigned	   &b )
{
	r = rgb >> 16 ;
	g = ( rgb >> 8 ) & 0xff ;
	b = rgb & 0xff ;
}

inline unsigned short make16( unsigned r, unsigned g, unsigned b )
{
	return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
}

static void writePNG( char const *fileName, void const *pngData, unsigned pngLength )
{
	FILE *fOut = fopen( fileName, "wb" );
	if( fOut ){
		fwrite( pngData, pngLength, 1, fOut );
		fclose( fOut );
		printf( "%u bytes written to %s\n", pngLength, fileName );
	}
	else
		perror( fileName );
}

static unsigned short mix( 
	unsigned short in16,
	unsigned char  opacity,
	unsigned char  highlight )
{
	unsigned short b = (((in16 & 0x1f))*opacity)/255;
	in16 >>= 5 ;
	unsigned short g = (((in16 & 0x3f))*opacity)/255;
	in16 >>= 6 ;
	unsigned short r = (((in16 & 0x1f))*opacity)/255;
	
	if( highlight )
	{
		// diffs from white
		unsigned short ldb = (0x1f-b);
		b += (ldb*highlight)/255 ;
		unsigned short ldg = (0x3f-g);
		g += (ldg*highlight)/255 ;
		unsigned short ldr = (0x1f-r);
		r += (ldr*highlight)/255 ;
	}
	
	unsigned short out16 = (r<<11)|(g<<5)|b ;
	return out16 ;
}

static void hframe( image_t const &img )
{
	unsigned short *pixels = (unsigned short *)img.pixData_ ;
	unsigned const rightShadowStart = img.width_-rightShadowWidth-1 ;

	for( unsigned row = 0 ; row < img.height_ ; row++ ){
		for( unsigned col = 0 ; col < img.width_ ; col++, pixels++ ){
			if( leftShadowWidth > col ){
				*pixels = 0 ;
			} else if(leftShadowWidth+leftHighlightWidth > col ){
				// linear highlight
				unsigned frac = ((col-leftShadowWidth+1)*255)/leftHighlightWidth ;
				*pixels = mix(*pixels,255,frac);
			} else if( col >= rightShadowStart ){
				// linear shadow
				unsigned offset = col - rightShadowStart ; // range 0..rightShadowWidth-1
				unsigned cover = rightShadowWidth-offset ; // range rightShadowWidth..1
				unsigned frac = (cover*255)/rightShadowWidth ;
				*pixels = mix(*pixels,frac,0);
			}
		}
	}
}

int main( int argc, char const *const argv[] )
{
	if( 3 < argc )
	{
		//
		// load font
		//	
		memFile_t fFont( argv[1] );
		if( !fFont.worked() ) {
			printf( "Error loading font\n" );
			return 0 ;
		}
	
		freeTypeFont_t font( fFont.getData(), fFont.getLength() );
		if( !font.worked() ){
			printf( "Error parsing font\n" );
			return 0 ;
		}

		unsigned pointSize = strtoul(argv[2],0,0);
		unsigned stripWidth  = strtoul(argv[3],0,0);

		//
		// Render an '8' to get font details
		//	
		freeTypeString_t biggest( font, pointSize, "8", 1, 256 );
		printf( "%u points, w:%u, h:%u, baseline:%d, yAdvance:%u\n",
			pointSize,
			biggest.getWidth(), 
			biggest.getHeight(),
			biggest.getBaseline(),
			biggest.getFontHeight() );

		if( stripWidth < biggest.getWidth() ){
			fprintf( stderr, "font data > width: %u:%u\n", stripWidth, biggest.getWidth() );
			return -1 ;
		}
		
		unsigned const rightMargin = (stripWidth-biggest.getWidth())/2 ;

		fbDevice_t &fb = getFB();

		// 
		// build digit strip
		// 
		unsigned const yAdvance = biggest.getFontHeight();
		topMargin    = (barHeight-yAdvance)/2;
		bottomMargin = barHeight-yAdvance-topMargin ;
		
		unsigned const stripHeight = 10*yAdvance ;
		unsigned const bottom = biggest.getBaseline() + (yAdvance-biggest.getHeight())/2 ;
		unsigned const numPixels = stripWidth*stripHeight;
		unsigned short *const pixels = new unsigned short [numPixels];
		unsigned const numBytes = numPixels*sizeof(pixels[0]);
	
		printf( "%u pixels, %u bytes /strip\n", numPixels, numBytes );
	
		unsigned short *digitTop = pixels ;
	
		unsigned rgb_fore = (5<argc) ? strtoul(argv[5], 0, 0 ) : 0 ;
		unsigned fgr, fgg, fgb ;
		rgbParts( rgb_fore, fgr, fgg, fgb );
		unsigned short fg16 = make16(fgr, fgg, fgb);
	
		unsigned rgb_back = (6<argc) ? strtoul(argv[6], 0, 0 ) : 0xE0E0E0 ;
		unsigned bgr, bgg, bgb ;
		rgbParts( rgb_back, bgr, bgg, bgb );
		unsigned short bg16 = make16(bgr, bgg, bgb);
	
		for( unsigned p = 0 ; p < numPixels ; p++ )
			pixels[p] = bg16 ;

		for( unsigned d = 0 ; d < 10 ; d++, digitTop += stripWidth*yAdvance ) {
			char st[2] = { '0'+d, 0 };
			freeTypeString_t digit( font, pointSize, st, 1, 256 );
			unsigned left = stripWidth-digit.getWidth()-rightMargin;
			printf( "%s:  w:%u, h:%u, baseline:%d, yAdvance:%u, left: %d\n",
				st,
				digit.getWidth(), 
				digit.getHeight(),
				digit.getBaseline(),
				digit.getFontHeight(),
				left );
			fb.antialias( digit.data_, digit.getWidth(), digit.getHeight(),
				      left, bottom-digit.getHeight(),
				      stripWidth, yAdvance,
				      fgr, fgg, fgb,
				      digitTop,
				      stripWidth,
				      yAdvance );
		}

		image_t stripImg( pixels, stripWidth, stripHeight );
		hframe( stripImg );
		void const *pngData ;
		unsigned    pngLength ;
		
		if( imageToPNG( stripImg, pngData, pngLength ) ){
			writePNG( "/tmp/digitStrip.png", pngData, pngLength );
		}
	
		//
		// build a dollar sign image
		//
		freeTypeString_t dollarSign( font, dollarPointSize, "$", 1, 256 );
		unsigned short *const dollarPix = new unsigned short [ dollarSign.getWidth()*yAdvance ];
		for( unsigned p = 0 ; p < dollarSign.getWidth()*yAdvance ; p++ )
			dollarPix[p] = bg16 ;
	
		fb.antialias( dollarSign.data_, 
			      dollarSign.getWidth(), dollarSign.getHeight(),
			      0, 10,
			      dollarSign.getWidth(), yAdvance,
			      fgr, fgg, fgb,
			      dollarPix,
			      dollarSign.getWidth(),
			      yAdvance );

		image_t dollarImg( dollarPix, dollarSign.getWidth(), yAdvance );
		hframe( dollarImg );

		if( imageToPNG( dollarImg, pngData, pngLength ) ){
			writePNG( "/tmp/dollarSign.png", pngData, pngLength );
		}

		//
		// and a decimal point image
		//
		unsigned short *const decimalPointPix = new unsigned short [decimalPointWidth*yAdvance];
		for( unsigned p = 0 ; p < decimalPointWidth*yAdvance ; p++ )
			decimalPointPix[p] = bg16 ;
		unsigned short *dpRow = decimalPointPix+((biggest.getBaseline()+topMargin)*decimalPointWidth) ;
		unsigned const dpMargin = (decimalPointWidth-decimalPointBlack)/2;
		for( unsigned y = 0 ; y < decimalPointBlack ; y++ )
		{
			for( unsigned x = 0 ; x < decimalPointBlack ; x++ )
				dpRow[x+dpMargin] = fg16 ;
			dpRow += decimalPointWidth ;
			
		}
	
		image_t decimalImg( decimalPointPix, decimalPointWidth, yAdvance );
		hframe(decimalImg);

		if( imageToPNG( decimalImg, pngData, pngLength ) ){
			writePNG( "/tmp/decimalPt.png", pngData, pngLength );
		}
	}
	else			//	 0    1     2      3           4                  5              6
		fprintf( stderr, "Usage: %s font ptsize digwidth [digheight=measured] [rgbfore=0] [rgbback=0xe0e0e0]\n", argv[0] );

	return 0 ;
}
