#include <time.h>
#include <stdio.h>
#include "entryFlash.h"

int main( int argc, char const * const argv[] ){
    if( 2 <= argc ){
        char const * const mtdName = argv[1];
        struct tagAndCount_t tc = {0};
        mtdCircular_t mtdDev(mtdName,&tc,validate_flash_entry_line);
        if( mtdDev.isOpen() ){
            struct tm tm ;
            localtime_r(&tc.when_, &tm);
            char time_buf[80];
            strftime(time_buf,sizeof(time_buf),"%Y-%m-%d %H:%M:%S", &tm);
            printf( "%s: %s, count %u\n", time_buf, tc.tag_, tc.numEntries_ );
        } else
            perror(mtdName);
    }
    else
        fprintf( stderr, "Usage: %s /dev/mtdx\n", argv[0]);
    return 0 ;
}
