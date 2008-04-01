/*
 * Module flashVar.cpp
 *
 * This module defines the readFlashVar() and writeFlashVar()
 * routines as declared in flashVar.h
 *
 *
 * Change History : 
 *
 * $Log: flashErase.cpp,v $
 * Revision 1.1  2008-04-01 18:58:55  ericn
 * -Initial import
 *
 * Revision 1.14  2007/07/16 18:36:15  ericn
 * -don't re-write duplicate variables
 *
 * Revision 1.13  2007/07/15 21:45:32  ericn
 * -added -erase command for testing
 *
 * Revision 1.12  2007/05/09 15:34:43  ericn
 * -change to make flashVar useful in scripts
 *
 * Revision 1.11  2006/02/13 21:12:49  ericn
 * -use 2.6 include files by default
 *
 * Revision 1.10  2006/12/01 18:37:43  tkisky
 * -allow variables to be saved to file
 *
 * Revision 1.9  2006/03/29 18:47:39  tkisky
 * -minor
 *
 * Revision 1.8  2005/11/22 17:41:58  tkisky
 * -fix write FLASHVARDEV use
 *
 * Revision 1.7  2005/11/16 14:57:42  ericn
 * -use FLASHVARDEV environment variable if present
 *
 * Revision 1.6  2005/11/16 14:49:44  ericn
 * -use debugPrint, not printf and comments
 *
 * Revision 1.5  2005/11/06 00:49:24  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.4  2005/08/12 04:18:43  ericn
 * -allow compile against 2.6
 *
 * Revision 1.3  2004/02/07 12:14:34  ericn
 * -allow zero-length values
 *
 * Revision 1.2  2004/02/03 06:11:12  ericn
 * -allow empty strings
 *
 * Revision 1.1  2004/02/01 09:53:18  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "flashVar.h"
#include <fcntl.h>
#include <time.h>
#include <sys/mount.h>
#ifndef KERNEL_2_4
#include <linux/types.h>
#include <mtd/mtd-user.h>
#else
#include <linux/mtd/mtd.h>
#endif
#include <syscall.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


int main( int argc, char const * const argv[] )
{
   if( 1 < argc ){
      char const *devName = argv[1];
      int fd = open( devName, O_RDWR );
      unsigned offset, maxSize ;
      if( 0 <= fd ){
         mtd_info_t meminfo;
         if( ioctl( fd, MEMGETINFO, (unsigned long)&meminfo) == 0)
         {
            printf( "flags       0x%lx\n", meminfo.flags ); 
            printf( "size        0x%lx\n", meminfo.size ); 
            printf( "erasesize   0x%lx\n", meminfo.erasesize ); 
            //	    printf( "oobblock    0x%lx\n", meminfo.oobblock ); 
            printf( "oobsize     0x%lx\n", meminfo.oobsize ); 
            printf( "ecctype     0x%lx\n", meminfo.ecctype ); 
            printf( "eccsize     0x%lx\n", meminfo.eccsize ); 
            printf( "numSectors  0x%lx\n", meminfo.size / meminfo.erasesize );
            erase_info_t erase;
            erase.start = 0 ;
            erase.length = meminfo.size ;
            if( 0 == ioctl( fd, MEMERASE, (unsigned long)&erase) )
            {
               printf( "%u bytes erased\n", meminfo.size );
            }
            else
               fprintf( stderr, "%s: erase error %m\n", devName );
         }
         else
            fprintf( stderr, "%s: GETINFO %m\n", devName );
      }
      else
         fprintf( stderr, "%s: %m\n", devName );
   }
   else
      fprintf( stderr, "Usage: %s /dev/mtd/3\n", argv[0] );

   return 0 ;
}

