#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

static bool readLine(FILE *f, char *buf, unsigned max) {
	bool inComment = false ;
	unsigned len = 0 ;
	while( len < max ) {
		char c ;
		if( 1 == fread(&c,1,1,f) ) {
			if( '\n' == c ) {
				break;
			} else if ('#' == c ) {
				inComment = true ;
			} else {
				if( !inComment )
					buf[len++] = c ;
			}
		}
		else
			return false ;
	}
	if( len < max ) {
		buf[len] = '\0' ;
		return true ;
	}
	else
		return false ;
}

static bool readHeader(FILE *f, unsigned &width, unsigned &height) {
	enum state_e {
		WANTFMT = 0,
		WANTSIZE = 1,
		WANTMAX = 2,
		DATA
	} state = WANTFMT ;

	while( state < DATA ) {
		char inBuf[256];
		if( readLine(f,inBuf,sizeof(inBuf)-1) ) {
			switch(state){
				case WANTFMT: {
					if('P' == inBuf[0])
						state = WANTSIZE ;
					else if( inBuf[0] ){
						fprintf(stderr, "Invalid data %s in state %d\n", inBuf, state );
						return false ;
					}
					break;
				}
				case WANTSIZE: {
					if( isdigit(inBuf[0]) ) {
						if( 2 == sscanf(inBuf,"%u %u", &width,&height) ) {
							state= WANTMAX ;
						} else {
							fprintf(stderr, "Invalid PBM size <%s>\n", inBuf);
							return false ;
						}
					}
					else if( inBuf[0] ){
						fprintf(stderr, "Invalid data %s in state %d\n", inBuf, state );
						return false ;
					}
					break;
				}
				case WANTMAX: {
					if( isdigit(inBuf[0]) ) {
						unsigned max ;
						if( 1 == sscanf(inBuf,"%u", &max) ) {
							state= DATA  ;
						} else {
							fprintf(stderr, "Invalid PBM max <%s>\n", inBuf);
							return false ;
						}
					}
					else if( inBuf[0] ){
						fprintf(stderr, "Invalid data %s in state %d\n", inBuf, state );
						return false ;
					}
					break;
				}
				default:
					fprintf(stderr, "Weird state %d\n", state );
					return false ;
			}
		}
		else
			return false ;
	}
	return (DATA == state);
	
}

int main(int argc, char const * const argv[])
{
	int rval = -1 ;
	if( 4 == argc ) {
		unsigned lwidth, lheight ;
		FILE *fLeft = fopen(argv[1],"rb");
		if( 0 == fLeft ){
			perror(argv[1]);
			return -1 ;
		}
		if( !readHeader(fLeft,lwidth,lheight) ){
			fprintf(stderr, "Error reading header from %s\n", argv[1]);
			return -1 ;
		}
		FILE *fRight = fopen(argv[2],"rb");
		if( 0 == fRight ){
			perror(argv[2]);
			return -1 ;
		}
		unsigned rwidth, rheight ;
		if( !readHeader(fRight,rwidth,rheight) ){
			fprintf(stderr, "Error reading header from %s\n", argv[2]);
			return -1 ;
		}
		if( (lwidth != rwidth) || (lheight != rheight) ){
			fprintf(stderr, "mismatched widths (%u.%u) or heights (%u.%u)\n", lwidth,rwidth,lheight,rheight);
			return -1 ;
		}
		printf( "compare %ux%u pixels\n", lwidth, lheight );
		unsigned imgSize= lwidth*lheight*3 ;
		unsigned char * const left = new unsigned char [imgSize];
		unsigned char * const right = new unsigned char [imgSize];
		unsigned char * const diff = new unsigned char [imgSize];
		if( imgSize != fread(left,1,imgSize,fLeft) ) {
			perror( "read(left)" );
			return -1 ;
		}
		if( imgSize != fread(right,1,imgSize,fRight) ) {
			perror( "read(right)" );
			return -1 ;
		}
		for( unsigned i = 0 ; i < imgSize ; i++ ){
			diff[i] = ~(left[i]^right[i]);
		}
		FILE *fOut = fopen(argv[3],"wb");
		if( fOut ){
			fprintf(fOut,"P6\n%u %u\n255\n", lwidth,lheight);
			fwrite(diff,1,imgSize,fOut);
			fclose(fOut);
			rval = 0 ;
		}
		else
			perror(argv[3]);
	} else 
		fprintf(stderr, "Usage: %s left.pbm right.pbm diff.pbm\n", argv[0] );
	return rval ;
}
