#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

unsigned char *pixData = 0 ;
unsigned width ;
unsigned height ;
unsigned stride ;

static void usage(void){
	fprintf(stderr, "Usage: bayer filename width height\n");
	exit(1);
}

static unsigned char *getRow(unsigned row) {
	if( row < height )
		return pixData+(stride*row);
}

static void cmdUsage(void) {
	fprintf(stderr, "Available commands:\n"
			"	pix row col			- show pixel value\n"
			"	pix row col R G B		- set pixel value\n"
			"	fill row col row col R G B 	- fill block of pixels\n" );
}

static void setPixel(unsigned row, unsigned col, int r, int g, int b )
{
	unsigned char *out = getRow(row);
	if( out && (col < width) ){
		out += col ;
		if(0 == (row & 1)) { 	// red row
			if(col&1) {  	// red column
				if( 0 <= r )
					*out = r ;
			} else {	// green column
				if( 0 <= g )
					*out = g ;
			}
		} else {		// blue row
			if(col&1) { 	// green column
				if( 0 <= g )
					*out = g ;
			} else {	// blue column
				if( 0 <= b )
					*out = b ;
			}
		}
	}
}

static void processCmdLine(char const *cmdline)
{
	char	cmd[80];
	int	args[8];
	cmd[0] = '\0' ;
	unsigned argc = sscanf(cmdline,"%s %u %i %i %i %i %i %i %i",
			       cmd, args, args+1, args+2, args+3,
			       args+4, args+5, args+6, args+7 );
	if( 1 < argc ){
		if( 0 == strcmp("pix",cmd) ){
			if( 3 == argc ) {
				unsigned char const *row = getRow(args[0]);
				if( row ) {
					if( (0 > args[1]) || (args[1] >= width) ) {
						fprintf(stderr, "Invalid column %d, max is %u\n", args[1],width);
						return;
					}
					row += args[1];
					unsigned char r, g, b ;
					if( 0 == (args[0]&1) ) { // red row
						if(0 == (args[1]&1)) {	// on green
							r = row[1];
							g = row[0];
							b = row[stride];
						} else {		// on red
							r = row[0];
							g = row[1];
							b = row[stride+1];
						}
					}
					else { // blue row
						if(0 == (args[1]&1)) {	// on blue
							r = row[stride+1];
							g = row[1];
							b = row[0];
						} else {		// on green
							r = row[stride];
							g = row[0];
							b = row[1];
						}
					}
					printf("%u:%u == %02x:%02x:%02x\n", args[0],args[1],r,g,b);
				}
			}
			else if( 6 == argc ) {
				printf("set pixel %u:%u to %d:%d:%d\n",
				       args[0],args[1],args[2],args[3],args[4]);
				unsigned char *row = getRow(args[0]);
				if( row ) {
					if( (0 > args[1]) || (args[1] >= width) ) {
						fprintf(stderr, "Invalid column %d, max is %u\n", args[1],width);
						return;
					}
					row += args[1];
					int newr=args[2], newg=args[3], newb=args[4];
					unsigned char oldr, oldg, oldb ;
					if( 0 == (args[0]&1) ) { // red row
						if( 0 <= newb )
							printf( "can't set blue on even rows\n");
						if(0 == (args[1]&1)) {	// on green
							oldr = row[1];
							oldg = row[0];
							oldb = row[stride];
							if( 0 <= newr )
								row[1] = newr ;
							if( 0 <= newg )
								row[0] = newg ;
						} else {		// on red
							oldr = row[0];
							oldg = row[1];
							oldb = row[stride+1];
							if( 0 <= newr )
								row[0] = newr ;
							if( 0 <= newg )
								row[1] = newg ;
						}
					}
					else { // blue row
						if( 0 <= newr )
							printf( "can't set red on odd rows\n" );
						if(0 == (args[1]&1)) {	// on blue
							oldr = row[stride+1];
							oldg = row[1];
							oldb = row[0];
							if( 0 <= newg )
								row[1] = newg ;
							if( 0 <= newb )
								row[0] = newb ;
						} else {		// on green
							oldr = row[stride];
							oldg = row[0];
							oldb = row[1];
							if( 0 <= newg )
								row[0] = newg ;
							if( 0 <= newb )
								row[1] = newb ;
						}
					}
					printf("%u:%u == 0x%02x:0x%02x:0x%02x -> ", args[0],args[1],oldr,oldg,oldb);
					printf("%d:%d:%d\n", newr,newg,newb);
				}
			}
			else {
				cmdUsage();
			}
		} else if( 0 == strcmp("fill", cmd)) {
			if( 8 == argc ){
				unsigned startc = args[0];
				unsigned endc = args[2];
				unsigned startr = args[1];
				unsigned endr = args[3];
				int r=args[4],g=args[5],b=args[6];
				if(startr > endr){
					unsigned tmp = startr ;
					startr = endr ;
					endr = tmp ;
				}
				if(startc > endc){
					unsigned tmp = startc ;
					startc = endc ;
					endc = tmp ;
				}
                                printf( "fill rectangle [%u:%u..%u:%u] with color %d:%d:%d here\n",
					startr,startc,endr,endc,r,g,b);
				if (endc >= width) {
					fprintf(stderr, "col %u out of range [0..%u]\n", endc, width-1 );
					return ;
				}
				if (endr >= height) {
					fprintf(stderr, "row %u out of range [0..%u]\n", endr, height-1 );
					return ;
				}
				for( unsigned row = startr ; row <= endr ; row++ ){
					for( unsigned col = startc ; col <= endc ; col++ ){
//						printf("[%u:%u] == %d/%d/%d\n", row, col, r, g, b );
						setPixel(row,col,r,g,b);
					}
				}
			}
			else
				cmdUsage();
		} else {
			fprintf(stderr, "unknown command <%s>\n", cmd );
			cmdUsage();
		}
	}
	else {
		fprintf(stderr, "%d parts parsed\n", argc );
		cmdUsage();
	}
}

static void trimBlanks(char *line)
{
	char *end = line+strlen(line);
	while(end > line) {
		char c = *(--end);
		if( 0x20 < c )
			break;
	}
	end[1] = '\0' ;
}

int main( int argc, char const * const argv[] )
{
	if( 4 <= argc ){
		char const *fileName = argv[1];
		width = strtoul(argv[2],0,0);
		stride = (width+31)&~31 ;
		height = strtoul(argv[3],0,0);
		if( 0 == width*height )
			usage();
		FILE *f = fopen(fileName, "r+");
		if( !f ) {
			perror(fileName);
			exit(1);
		}
		pixData = new unsigned char [stride*height];
		int numRead = fread(pixData,1,width*height,f);
		for( int arg = 4 ; arg < argc ; arg++ ) {
			processCmdLine(argv[arg]);
		}
		char cmdLine[256];
		while(fgets(cmdLine,sizeof(cmdLine),stdin)) {
			trimBlanks(cmdLine);
			processCmdLine(cmdLine);
		}
		ftruncate(fileno(f),0);
		fseek(f,0,SEEK_SET);
		fwrite(pixData,1,stride*height,f);
		fclose(f);
	}
	else
		usage();
	return 0 ;
}
