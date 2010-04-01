#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

inline unsigned char red(unsigned short rgb565){
   unsigned char r = (rgb565 & (0x1f<<11)) >> (11-3);
   if( r & 0x80 )
      r |= 7 ;       // more red if high red
   return r ;
}
inline unsigned char green(unsigned short rgb565){
   unsigned char g = (rgb565 & (0x3f<<5)) >> (5-2);
   if( g & 0x80 )
      g |= 3 ;       // more green if high green
   return g ;
}
inline unsigned char blue(unsigned short rgb565){
   unsigned char b = (rgb565 & 0x1f) << 3 ;
   if( b & 0x80 )
      b |= 7 ;       // more blue if high blue
   return b ;
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
		if(fileSize == (width*height*2)){
			printf( "%ux%u == %ld\n", width, height, fileSize );
			unsigned short *inData = new unsigned short[fileSize/2];
			int numRead = read(fdIn,inData,fileSize);
			if( fileSize == numRead ) {
				FILE *fOut = fopen(argv[4],"w");
				if(fOut) {
					fprintf(fOut, "P6\n%u %u\n255\n", width, height );
					for( unsigned row = 0 ; row < height ; row++ ) {
						for( unsigned col = 0 ; col < width ; col++ ) {
                            unsigned short const rgb565  = *inData++ ;
							unsigned char r = red(rgb565);
							unsigned char g = green(rgb565);
							unsigned char b = blue(rgb565);
                            if( 0 && rgb565 ){
                                printf( "%04x -> %02x.%02x.%02x\n", rgb565,r,g,b);
                            }
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
			fprintf(stderr, "Invalid size %ld for %ux%u image (is it RGB565?)\n", fileSize, width, height );
		close(fdIn);
	} else
		fprintf(stderr, "Usage: %s infile w h outfile\n", argv[0] );

	return rval ;
}
