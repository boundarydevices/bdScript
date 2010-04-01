#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define cameraStride(w) ((w+31)&~31)

inline unsigned char red(unsigned char const *inData,unsigned w,unsigned row, unsigned col) {
#ifdef NEAREST_NEIGHBOR
	inData += w*(row&~1);
	return inData[col|1];
#else
	inData += (w*row)+col;
	if(row&1) {	// blue row 
		unsigned char const *above = inData-w ;
		unsigned char const *below = inData+w ;
		if(col&1) {		// average above and below
			return (*above+*below)>>1 ;
		} else {		// average four (ul/ur/ll/lr)
                        if(col) {
				return (above[-1]+above[1]+below[-1]+below[1])>>2 ;
			} else {	// average two (ur/lr)
				return (above[1]+below[1])>>1 ;
			}
		}
	} else {	// red row
		if(col&1)
			return *inData ;
		else if( col )
			return (inData[-1]+inData[1])>>1 ; // average left and right
		else
			return inData[1]; // leftmost pixel
	}
#endif
}

inline unsigned char green(unsigned char const *inData,unsigned w,unsigned row, unsigned col) {
#ifdef NEAREST_NEIGHBOR
	inData += w*row;
	if(row&1)
		return inData[col|1];
	else
		return inData[col&~1];
#else
	inData += w*row+col ;
	if ((row&1)==(col&1)) { // on green
		return *inData ;
	} else {
		// three case here
		//	normal: row > 0 && col > 0		- average four pixels
		//	top: row == 0, col > 0			- average left and right
		//	left: row > 0, col == 0			- average above and below
		if (0 == row) {
			return (inData[-1]+inData[1])>>1;
		} else {
			unsigned char const *above = inData-w ;
			unsigned char const *below = inData+w ;
			if (0 == col) {
				return (*above+*below)>>1 ;
			} else
				return (*above+*below+inData[-1]+inData[1])>>2 ;
		}
	}
#endif
}

inline unsigned char blue(unsigned char const *inData,unsigned w,unsigned row, unsigned col) {
#ifdef NEAREST_NEIGHBOR
	inData += w*(row|1);
	return inData[col&~1];
#else
	inData += w*row+col ;
	if(row&1) {	// blue row
		if (col&1){ 	// green pixel: average left and right
			return (inData[-1]+inData[1]) >> 1 ;
		} else {
			return *inData ;
		}
	} else {	// red row
		unsigned char const *above = inData-w ;
		unsigned char const *below = inData+w ;
		if( 0 == row )
			above = below ;
		if (col&1) {	// average four (ul,ur,ll,lr)
			return (above[-1]+above[1]+below[-1]+below[1])>>2 ;
		} else {	// average two (above and below)
			return (*above+*below)>>1 ;
		}
	}
#endif
}

int main( int argc, char const * const argv[] )
{
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
				FILE *fOut = fopen(argv[4],"w");
				if(fOut) {
					fprintf(fOut, "P6\n%u %u\n255\n", width, height );
					for( unsigned row = 0 ; row < height ; row++ ) {
						for( unsigned col = 0 ; col < width ; col++ ) {
							unsigned char r = red(inData,stride,row,col);
							unsigned char g = green(inData,stride,row,col);
							unsigned char b = blue(inData,stride,row,col);
							fwrite(&r,1,1,fOut);
							fwrite(&g,1,1,fOut);
							fwrite(&b,1,1,fOut);
						}
					}
					fclose(fOut);
					rval = 0 ;
				}
				else
					perror(argv[4]);
			} else
				perror("read(fdIn)");
		} else
			fprintf(stderr, "Invalid size %ld for %ux%u image (is it 8-bit Bayer?)\n", fileSize, width, height );
		close(fdIn);
	} else
		fprintf(stderr, "Usage: %s infile w h outfile\n", argv[0] );

	return rval ;
}
