/*
 * Program mkFlash.cpp
 *
 * This program builds a totalflash image out of
 * separate kernel and filesystem images.
 *
 * Pretty simple, really. It just pads the kernel
 * to 1MB with 0xFF's and plops the filesystem image
 * at the end.
 *
 *
 * Change History : 
 *
 * $Log: mkFlash.cpp,v $
 * Revision 1.1  2004-03-17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "memFile.h"
#include <stdio.h>
#include <string.h>

#define ONEMB (1<<20)
#define QUARTERMB (ONEMB/4) 

int main( int argc, char const * const argv[] )
{
   if( 4 == argc )
   {
      memFile_t fKernel( argv[1] );
      if( fKernel.worked() )
      {
         if( ( ONEMB >= fKernel.getLength() )
             &&
             ( QUARTERMB <= fKernel.getLength() ) )
         {
            memFile_t fFileSys( argv[2] );
            if( fFileSys.worked() )
            {
               FILE *fOut = fopen( argv[3], "wb" );
               if( fOut )
               {
                  int numWritten = fwrite( fKernel.getData(), 1, fKernel.getLength(), fOut );
                  if( numWritten == fKernel.getLength() )
                  {
                     printf( "0x%x bytes of kernel\n", numWritten );
                     if( ONEMB > fKernel.getLength() )
                     {
                        unsigned const numFill = ONEMB-numWritten ;
                        unsigned char *const fill = new unsigned char[ numFill ];
                        memset( fill, 0xFF, numFill );
                        numWritten = fwrite( fill, 1, numFill, fOut );
                        printf( "0x%x bytes of filler\n", numFill );
                     }

                     numWritten = fwrite( fFileSys.getData(), 1, fFileSys.getLength(), fOut );
                     if( numWritten == fFileSys.getLength() )
                     {
                        printf( "0x%x bytes of filesystem\n", numWritten );
                        long const fileSize = ftell( fOut );
                        printf( "resulting file is 0x%x (%lu) bytes\n", fileSize, fileSize );
                     }
                     else
                        perror( argv[3] );
                  }
                  else
                     perror( argv[3] );
                  fclose( fOut );
               }
               else
                  perror( argv[3] );
            }
            else
               perror( argv[2] );
         }
         else
            fprintf( stderr, "Max kernel size is 1MB\n" );
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s bd2003Kernel bd2003Files output.flash\n", argv[0] );

   return 0 ;
}

