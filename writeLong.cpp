/*
 * Program writeLong.cpp
 *
 * This program will write a long-word value to a specified 
 * offset within the file or device specified on the 
 * command-line. The old value will be displayed on stdout 
 * (so it can be used in a shell script).
 *
 * Change History : 
 *
 * $Log: writeLong.cpp,v $
 * Revision 1.1  2007-05-23 23:11:11  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>

static bool useDecimal = false ;

static void parseArgs( int &argc, char const **argv )
{
	for( int i = 1 ; i < argc ; i++ ){
		char const *arg = argv[i];
		if( '-' == *arg ){
			arg++ ;
			while( *arg ){
				char const c = toupper(*arg++);
				if( 'D' == c ){
					useDecimal = true ;
				} else
					fprintf( stderr, "Invalid flag  -%c\n", c );
			}

			// pull from argument list
			for( int j = i+1 ; j < argc ; j++ ){
				argv[j-1] = argv[j];
			}
			--i ;
			--argc ;
		}
	}
}

int main( int argc, char const *argv[] )
{
	int rval = -1 ;
	parseArgs(argc,argv);
	if( 4 <= argc ){
		int const fd = open( argv[1], O_RDWR );
		if( 0 <= fd ){
			unsigned long offset = strtoul(argv[2], 0, 0);
			unsigned long newValue = strtoul(argv[3], 0, 0 );
			int result = lseek( fd, offset, SEEK_SET );
			if( (unsigned long)result == offset ){
				unsigned long oldValue ;
                                int numRead = read( fd, &oldValue, sizeof( oldValue ) );
				if( sizeof(oldValue) == numRead ){
					result = lseek( fd, offset, SEEK_SET );
					if( (unsigned long)result == offset ){
						int numWritten = write( fd, &newValue, sizeof(newValue));
						if( numWritten == sizeof(newValue) ){
							printf( useDecimal ? "%ld" : "%08lx", oldValue );
							rval = 0 ;
						}
						else
							perror( "write" );
					}
					else
						perror( "seek2" );
				}
				else
					perror( "read" );
			}
			else
				perror( "seek" );
			close( fd );
		}
		else
			perror( argv[1] );
	}
	else
		fprintf( stderr, "Usage: %s [-d] fileName offset value\n"
	       			 "   values in decimal or 0xHEX\n"
				 "   defaults to hex display\n"
				 "   use -d for decimal\n", argv[0] );

	return rval ;
}
