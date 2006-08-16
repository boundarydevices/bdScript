#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/sm501mem.h>
#include <string.h>
#include <errno.h>
#include "tickMs.h"

int main( int argc, char const * const argv[] )
{
   int const fd = open( "/dev/sm501mem", O_RDWR );
   if( 0 <= fd ) {
      if( 1 == argc ){
         unsigned long totalTicks = 0 ;
         unsigned totalAlloc = 0 ;
         unsigned i = 0 ;
         while( 1 ){
            unsigned const allocSize = (i & 7) + 1 ;
            unsigned long size = allocSize ;
            long long start = tickMs();
            int rval = ioctl( fd, SM501_ALLOC, &size );
            long long end = tickMs();
            if( 0 == rval ){
               totalTicks += (unsigned long)(end-start);
               if( size ){
   //               printf( "alloc %u: 0x%lx\n", allocSize, size );
                  totalAlloc += allocSize ;
                  i++ ;
   
   #if 0
                  if( 8 == allocSize ){
                     rval = ioctl( fd, SM501_FREE, &size );
                     if( 0 != rval ){
                        printf( "free error\n" );
                        break ;
                     }
                  }
   #endif
               }
               else {
                  printf( "NULL pointer returned\n" );
                  break ;
               }
            }
            else {
               perror( "SM501_ALLOC" );
               break ;
            }
         }
         printf( "%u bytes allocated in %u iterations, %lu ms\n",
                 totalAlloc, i, totalTicks );
      }
      else {
         unsigned count = strtoul(argv[1],0,0);
         if( count ){
            unsigned long *const allocations = new unsigned long [count];

            unsigned long totalTicks = 0 ;
            unsigned totalAlloc = 0 ;
            unsigned i = 0 ;
            while( i < count ){
               unsigned const allocSize = (i & 7) + 1 ;
               unsigned long size = allocSize ;
               long long start = tickMs();
               int rval = ioctl( fd, SM501_ALLOC, &size );
               long long end = tickMs();
               if( 0 == rval ){
                  totalTicks += (unsigned long)(end-start);
                  if( size ){
      //               printf( "alloc %u: 0x%lx\n", allocSize, size );
                     totalAlloc += allocSize ;
                     allocations[i++] = size ;
                  }
                  else {
                     printf( "NULL pointer returned\n" );
                     break ;
                  }
               }
               else {
                  perror( "SM501_ALLOC" );
                  break ;
               }
            }
            printf( "%u bytes allocated in %u iterations, %lu ms\n",
                    totalAlloc, i, totalTicks );

            count = i ;
            totalTicks = 0 ;
            for( i = 0 ; i < count-1 ; i++ )
            {
/*
               if( 1 == i )
                  printf( "sm501free: %lx\n", allocations[0] );
printf( "sm501free: %lx\n", allocations[i] );
*/
               long long start = tickMs();
               int rval = ioctl( fd, SM501_FREE, allocations+i );
               long long end = tickMs();
               if( 0 == rval ){
                  totalTicks += (unsigned long)(end-start);
               } else {
                  perror( "SM501_FREE" );
                  break ;
               }
            }
            printf( "freed in %lu ms\n", totalTicks );

/*
            FILE *fProc = fopen( "/proc/driver/sm501mem", "rb" );
            if( fProc ){
               char inBuf[4096];
               int numRead ;
               while( 0 < ( numRead = fread( inBuf, 1, sizeof(inBuf), fProc ) ) )
                  fwrite( inBuf, 1, numRead, stdout );
               fclose( fProc );
            }
*/            
         }
         else
            fprintf( stderr, "Usage: sm501mem [itercount]\n" );
      }
      

      close( fd );
   }
   else
      perror( "/dev/fb/0" );

	return 0 ;
}

