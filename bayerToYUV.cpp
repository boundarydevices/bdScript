/*
 * Module bayerToYUV.cpp
 *
 * This module defines the bayerToYUV() routine as declared in 
 * bayerToYUV.h.
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "bayerToYUV.h"
#include <alloca.h>
#include <string.h>
#include <stdlib.h>

/*
 *
 * Convert from Bayer to planar YUV with bi-linear interpolation
 *
 *	from :	    GRGRGRGR		to:   YYYYYYYY
 *		    BGBGBGBG                  YYYYYYYY       UUUU       VVVV 
 *                  GRGRGRGR                  YYYYYYYY       UUUU       VVVV 
 *                  BGBGBGBG                  YYYYYYYY
 *
 * There are two primary parts of this algorithm:
 *	- produce interpolated R' G' B' for each output pixel
 *	- convert R' G' and B' to Y for each pixel
 *			       to U and V for every other pixel
 * Input (and interpolation)
 *
 * 	Algorithm walks rows two at a time, reading four lines of input
 *		top_in
 *		cur_in
 *		bot_in
 *		bbt_in
 *
 *	and keeps track of four samples of each line
 *		ltop/top/rtop/rrtop		(left,current,right,right-of-right)
 *		lcur/cur/rcur/rrcur
 *		lbot/bot/rbot/rrbot
 *		lbbt/bbt/rbbt/rrbbt
 *
 *	First row uses next_in as prev_in so it may generate some edge artifacts
 *	Last row uses prev_in as next_in...
 *
 */

#include <assert.h>
#include <stdio.h>
#include <alloca.h>
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#if 0
inline void saveRGB
	( unsigned char *rgbOut,
	  unsigned	 row,
	  unsigned	 col,
	  unsigned	 width,
	  unsigned char  r,
	  unsigned char  g,
	  unsigned char  b,
	  unsigned char  which ) {
	if (rgbOut) {
		unsigned stride = 3*width ;
		unsigned char *out = rgbOut+(row*stride)+(col*3);
		out += (which>>1)*stride ;
		out += 3*(which&1);
                *out++ = r ; 
		*out++ = g ; 
		*out   = b ; 
	}
}
#define SAVERGB(r,g,b,which) if(rgbOut) saveRGB(rgbOut,row,col,inWidth,r,g,b,which); else printf( "no rgb\n" )
#else
#define SAVERGB(r,g,b,which)
#endif

#define BYTE0(lw) ((unsigned char)lw)
#define BYTE1(lw) ((unsigned char)(lw>>8))
#define BYTE2(lw) ((unsigned char)(lw>>16))
#define BYTE3(lw) ((unsigned char)(lw>>24))

void bayerToYUV
	( unsigned char const *bayerIn, 
	  unsigned inWidth,
	  unsigned inHeight,
	  unsigned inStride,
	  unsigned char *yOut,
	  unsigned char *uOut,
	  unsigned char *vOut,
	  unsigned	 yStride,
	  unsigned	 uvStride,
	  unsigned char *rgbOut )
{
	assert(0 == (inStride&3));
	assert(0 == (yStride&3));
	assert(0 == (uvStride&3));
	assert(0 == ((int)bayerIn&3));
	assert(0 == ((int)yOut&3));
	assert(0 == ((int)uOut&3));
	assert(0 == ((int)vOut&3));
	assert(0 == (inHeight&1));

//	unsigned char * const blackRow = (unsigned char *)alloca(inStride);
//	memset(blackRow,0,inStride);

	unsigned char const *top_in = bayerIn+inStride ; // blackRow ;
	unsigned char const *cur_in = bayerIn ;
	unsigned char const *bot_in = cur_in+inStride ;
	unsigned char const *bbt_in = bot_in+inStride ;

// 	unsigned char rvals[4],gvals[4],bvals[4];
	for( unsigned row = 0 ; row < inHeight ; row += 2 ) {
// printf( "%3u:%p.%p.%p.%p\n", row,top_in,cur_in,bot_in,bbt_in);
		// presume black border along left edge
		unsigned long top = *((unsigned long *)top_in);      top_in += 4 ; 
		unsigned long cur = *((unsigned long *)cur_in);      cur_in += 4 ; 
		unsigned long bot = *((unsigned long *)bot_in);      bot_in += 4 ; 
		unsigned long bbt = *((unsigned long *)bbt_in);      bbt_in += 4 ; 

		unsigned char ltop = top >> 8 ;
		unsigned char lcur = cur >> 8 ;
		unsigned char lbot = bot >> 8 ;
		unsigned char lbbt = bbt >> 8 ;

		unsigned long rtop = *((unsigned long *)top_in);     top_in += 4 ;
		unsigned long rcur = *((unsigned long *)cur_in);     cur_in += 4 ;
		unsigned long rbot = *((unsigned long *)bot_in);     bot_in += 4 ;
		unsigned long rbbt = *((unsigned long *)bbt_in);     bbt_in += 4 ;

		for( unsigned col = 0 ; col < inWidth ; col += 2 ) {
			unsigned char r, g, b ; 
			unsigned y, u, v ;
			// process red row 2 at a time
			//	g rlr btb	g4 r b4		
			g = BYTE0(cur);
			r = (lcur+BYTE1(cur))>>1 ;
			b = (BYTE0(top)+BYTE0(bot))>>1 ;

			SAVERGB(r,g,b,0);
			// intel y = (9798*r + 19235*g + 3736*b) / 32768 ;
			y = (9798*r + 19235*g + 3736*b) / 32768 ; 
			yOut[col] = y ;

			g = (g+BYTE1(top)+BYTE1(bot)+BYTE2(cur)) >> 2 ;
			r = BYTE1(cur);
			b = (BYTE0(top)+BYTE0(bot)+BYTE2(top)+BYTE2(bot)) >> 2 ;

			SAVERGB(r,g,b,1);
			// intel y = (9798*r + 19235*g + 3736*b) / 32768 ;
			y = (9798*r + 19235*g + 3736*b) / 32768 ; 
			yOut[col+1] = y ;

			// process blue row 2 at a time
			//	g4 r4 b		g  rtb blr
			g = (BYTE0(cur)+lbot+BYTE1(bot)+BYTE0(bbt))>>2 ;
			r = (lcur+lbbt+BYTE1(cur)+BYTE1(bbt)) >>2 ;
			b = BYTE0(bot);

			SAVERGB(r,g,b,2);
			// intel y = (9798*r + 19235*g + 3736*b) / 32768 ;
			y = (9798*r + 19235*g + 3736*b) / 32768 ; 
			yOut[col+yStride] = y ;

			g = BYTE1(bot);
			r = (BYTE1(cur)+BYTE1(bbt))>>1 ;
			b = (BYTE0(bot)+BYTE2(bot))>>1 ;

			SAVERGB(r,g,b,3);
			// intel y = (9798*r + 19235*g + 3736*b) / 32768 ;
			y = (9798*r + 19235*g + 3736*b) / 32768 ; 
			yOut[col+yStride+1] = y ;

			// convert U and V
// U = [(-4784 R - 9437 G + 4221 B) / 32768] + 128
// V = [(20218R - 16941G - 3277 B) / 32768] + 128
			r = BYTE1(cur); g=(BYTE0(cur)+BYTE1(bot))>>1 ; b = BYTE0(bot);

			// intel u = (((-4784*(int)r) - (9437*(int)g) + (4221*(int)b)) / 32768) + 128 ;
                        u = (((-5529*(int)r) - (10855*(int)g) + (16384*(int)b)) / 32768) + 128 ;
			if( u > 255 ) {
				printf( "u overflow: %x (%02x:%02x:%02x)\n", u, r, g, b );
				u = 255 ;
			}
			// intel v = ((20218*(int)r - 16941*(int)g - 3277*(int)b) / 32768) + 128 ;
                        v = ((16384*(int)r - 13720*(int)g - 2664*(int)b) / 32768) + 128 ;
			uOut[col>>1] = u ;
			vOut[col>>1] = v ;

			// advance
			ltop = top >> 8 ;
			lcur = cur >> 8 ;
			lbot = bot >> 8 ;
			lbbt = bbt >> 8 ;

			top = (top>>16) | (((unsigned short)rtop)<<16);
			cur = (cur>>16) | (((unsigned short)rcur)<<16);
			bot = (bot>>16) | (((unsigned short)rbot)<<16);
			bbt = (bbt>>16) | (((unsigned short)rbbt)<<16);

			if(col & 2) {	// read next longwords
				if( unlikely(col+4 > inWidth) ) {
					rtop = rcur = rbot = rbbt = 0 ;
				} else {
					rtop = *((unsigned long *)top_in);     top_in += 4 ; 
					rcur = *((unsigned long *)cur_in);     cur_in += 4 ; 
					rbot = *((unsigned long *)bot_in);     bot_in += 4 ; 
					rbbt = *((unsigned long *)bbt_in);     bbt_in += 4 ; 
				}
			} else { 
				rtop = (rtop>>16);
				rcur = (rcur>>16);
				rbot = (rbot>>16);
				rbbt = (rbbt>>16);
			}
		}

		top_in -= (inWidth+4) ; 
		cur_in -= (inWidth+4) ; 
		bot_in -= (inWidth+4) ; 
		bbt_in -= (inWidth+4) ; 

		// advance by 2
		top_in = bot_in ;
		cur_in = bbt_in ;
		if( row+4 >= inHeight ){
			bot_in = top_in ;
			bbt_in = cur_in ;
		} else {
			bot_in = bbt_in+inStride ;
			bbt_in = bot_in+inStride ;
		}
		yOut += 2*yStride ;
		uOut += uvStride ;
		vOut += uvStride ;
	}
}

#ifdef MODULETEST

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "tickMs.h"
#include <ctype.h>

#define cameraStride(w) ((w+31)&~31)

static char const *rgbFile = 0 ;
unsigned iterations = 1 ;

static void parseArgs( int &argc, char const **argv )
{
	for( unsigned arg = 1 ; arg < argc ; arg++ ){
		if( '-' == *argv[arg] ){
			char const *param = argv[arg]+1 ;
			if( 'r' == tolower(*param) ){
            			rgbFile= param+1 ;
			}
			else if( 'i' == tolower(*param) ){
            			iterations = strtoul(param+1,0,0);
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
	}
}

int main( int argc, char const **argv )
{
	parseArgs(argc,argv);

	int rval = -1 ;
	if( 4 < argc ) {
		int fdIn = open(argv[1],O_RDONLY);
		if( 0 > fdIn ){
			perror(argv[1]);
			return -1 ;
		}
		printf( "%s opened\n", argv[1] );
		long int fileSize = lseek(fdIn,0,SEEK_END);
		lseek(fdIn,0,SEEK_SET);
		unsigned width = strtoul(argv[2],0,0);
		unsigned height = strtoul(argv[3],0,0);
		unsigned stride = cameraStride(width);
		if(fileSize == (stride*height)){
			printf( "%ux%u == %ld\n", width, height, fileSize );
			unsigned char *const inData = new unsigned char[fileSize];
			int numRead = read(fdIn,inData,fileSize);
			if( fileSize == numRead ) {
				unsigned yStride = (width+15)&~15 ;
				unsigned uvStride = ((width/2)+7)&~7 ;
				unsigned yuvSize = (height*(yStride+uvStride));
				unsigned ySize = height*yStride ;
				unsigned uvSize = ((height*uvStride)/2);
				unsigned char *const yuvBuf = new unsigned char [yuvSize];
				unsigned char *const u = yuvBuf+ySize;
				unsigned char *const v = u+uvSize;
memset(u,0x80,uvSize);
memset(v,0x55,uvSize);
				printf( "process output here\n"
					"%ux%u (inStride %u) -> %u bytes of y (stride %u), %u bytes of u and v (stride %u)\n",
					width, height, stride,
					ySize, yStride, uvSize, uvStride );
				unsigned char *rgbOut = 0 ;
				unsigned rgbSize = 3*width*height ;
				if( rgbFile ) {
					rgbOut = new unsigned char [rgbSize];
					memcpy(rgbOut,inData,fileSize);
				}
				long long start = tickMs();
				for(unsigned i = 0 ; i < iterations ; i++) {
					bayerToYUV(inData,width,height,stride,
						   yuvBuf,u,v,yStride,uvStride,rgbOut);
				}
				long long end = tickMs();
				printf( "%u conversions in %llu ms\n", iterations, end-start);
				FILE *fOut = fopen(argv[4],"wb");
				if(fOut) {
					fwrite(yuvBuf,yuvSize,1,fOut);
					fclose(fOut);
				} else
					perror(argv[4]);
				if( rgbFile ) {
					fOut = fopen(rgbFile,"wb");
					if(fOut) {
						fprintf(fOut, "P6\n%u %u\n255\n", width, height );
						fwrite(rgbOut,1,rgbSize,fOut);
						fclose(fOut);
					}
					else
						perror(rgbFile);
				}
			} else
				perror("read(fdIn)");
		} else
			fprintf(stderr, "Invalid size %ld for %ux%u image (is it 8-bit Bayer?)\n", fileSize, width, height );
		close(fdIn);
	} else
		fprintf(stderr, "Usage: %s infile w h yuvfile [rgbfile]\n", argv[0] );

	return rval ;
}

#endif
