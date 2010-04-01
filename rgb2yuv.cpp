#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const * const argv[] )
{
	if( 4 == argc ){
		unsigned char r = strtoul(argv[1],0,0);
		unsigned char g = strtoul(argv[2],0,0);
		unsigned char b = strtoul(argv[3],0,0);
		unsigned y = (9798*r + 19235*g + 3736*b) / 32768 ;
		unsigned u = (((-5529*(int)r) - (10855*(int)g) + (16384*(int)b)) / 32768) + 128 ;
		unsigned v = ((16384*(int)r - 13720*(int)g - 2664*(int)b) / 32768) + 128 ;
		printf( "rgb(%u,%u,%u) == yuv(%u,%u,%u) 0x%02x,0x%02x,0x%02x\n", r,g,b,y,u,v,y,u,v);
	}
	else
		fprintf(stderr, "Usage: %s r(0..255) g(0..255) b(0..255)\n", argv[0]);
	return 0 ;
}

