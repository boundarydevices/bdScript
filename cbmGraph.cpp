/*
 * Module cbmGraph.cpp
 *
 * This module defines ...
 *
 * Todo:
 *
 *    Test buffer size (see effect on stalls)
 *    Test line spacing
 *    Integrate images
 *
 * Change History : 
 *
 * $Log: cbmGraph.cpp,v $
 * Revision 1.2  2003-05-10 03:17:47  ericn
 * -changed cut command
 *
 * Revision 1.1  2003/05/09 04:28:12  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "cbmGraph.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/lp.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <assert.h>

/* ioctls: */
#define LPGETSTATUS		0x060b		/* same as in drivers/char/lp.c */
#define IOCNR_GET_DEVICE_ID		1
#define IOCNR_GET_PROTOCOLS		2
#define IOCNR_SET_PROTOCOL		3
#define IOCNR_HP_SET_CHANNEL		4
#define IOCNR_GET_BUS_ADDRESS		5
#define IOCNR_GET_VID_PID		6
/* Get device_id string: */
#define LPIOC_GET_DEVICE_ID(len) _IOC(_IOC_READ, 'P', IOCNR_GET_DEVICE_ID, len)
/* The following ioctls were added for http://hpoj.sourceforge.net: */
/* Get two-int array:
 * [0]=current protocol (1=7/1/1, 2=7/1/2, 3=7/1/3),
 * [1]=supported protocol mask (mask&(1<<n)!=0 means 7/1/n supported): */
#define LPIOC_GET_PROTOCOLS(len) _IOC(_IOC_READ, 'P', IOCNR_GET_PROTOCOLS, len)
/* Set protocol (arg: 1=7/1/1, 2=7/1/2, 3=7/1/3): */
#define LPIOC_SET_PROTOCOL _IOC(_IOC_WRITE, 'P', IOCNR_SET_PROTOCOL, 0)
/* Set channel number (HP Vendor-specific command): */
#define LPIOC_HP_SET_CHANNEL _IOC(_IOC_WRITE, 'P', IOCNR_HP_SET_CHANNEL, 0)
/* Get two-int array: [0]=bus number, [1]=device address: */
#define LPIOC_GET_BUS_ADDRESS(len) _IOC(_IOC_READ, 'P', IOCNR_GET_BUS_ADDRESS, len)
/* Get two-int array: [0]=vendor ID, [1]=product ID: */
#define LPIOC_GET_VID_PID(len) _IOC(_IOC_READ, 'P', IOCNR_GET_VID_PID, len)

#define MAXDEVICE 1024

int main( void )
{
   printf( "Hello, world\n" );
   int const fd = open( "/dev/usb/lp0", O_WRONLY );
   if( 0 <= fd )
   {
      printf( "printer port opened\n" );

      unsigned const width = 408 ;
      unsigned const wBits = ((width+7)/8)*8 ;
      unsigned const height = 240 ;
      unsigned const hBytes = (height+7)/8 ;
      unsigned const headerLen = 4 ;
      unsigned const trailerLen = 7 ;
      char * const outBuf = new char [ headerLen+trailerLen + wBits*hBytes ];
      char *nextOut = outBuf ;

      nextOut += sprintf( outBuf, "\x1d*%c%c", wBits/8, hBytes );

      for( unsigned y = 0 ; y < wBits ; y++ )
      {
         for( unsigned x = 0 ; x < hBytes ; x++ )
         {
            *nextOut++ = ( y & 1 ) ? 0x55 : 0xaa ;
         }
      }
      nextOut += sprintf( nextOut, "\x1d/%c\n\n\n\n", 0 );

      int const numWritten = write( fd, outBuf, nextOut-outBuf );
      printf( "wrote %d bytes\n", numWritten );

      write( fd, "\x1dV", 6 );

/*
      int const width = 410 ;
      int const n1 = width & 0xFF ;
      int const n2 = width >> 8 ;
      int const headerLen = 5 ;
      int const trailerLen = 3 ;

      unsigned const byteCount = (n1 + (n2<<8))*3 + headerLen + trailerLen ;
      char *const temp = new char [byteCount+1];
      char *nextOut = temp ;
      nextOut += sprintf( nextOut, "\x1b*!%c%c", n1, n2 );

      nextOut += sprintf( nextOut, "\xff\xff\xff" ); // left edge
      for( int i = 1 ; i <= width-2 ; i++ )
      {
         *nextOut++ = '\xaa' ; // top 
         *nextOut++ = '\xaa' ; // middle
         *nextOut++ = '\xaa' ; // bottom
      }

      nextOut += sprintf( nextOut, "\xff\xff\xff" ); // right edge

      // print and feed
      *nextOut++ = '\x1b' ;
      *nextOut++ = 'J' ;
      *nextOut++ = 24 ;    // (24/203)*360
      
      int const len = write( fd, temp, byteCount );
      if( len != byteCount )
      {
         fprintf( stderr, "error %d(%m) drawing box\n", len );
      }
*/
/*
      int len = sprintf( temp, "\x1d*%c%c", n1, n2 );
      write( fd, temp, len );
      for( int j = 1 ; j <= n1*8 ; j++ )
      {
         for( int i = 1 ; i <= n2 ; i++ )
         {
            char const c = (char)j ;
            write( fd, &c, sizeof( c ) );
         }
      }
      write( fd, "\x1d/\x00", 3 );
      write( fd, "\x1d/\x01", 3 );
      write( fd, "\x1d/\x02", 3 );
      write( fd, "\x1d/\x03", 3 );
*/
      printf( "closing port\n" );
      fflush( stdout );
      fflush( stderr );
      fsync( fd );
      close( fd );
   }
   else
      perror( "/dev/usb/lp0" );
   
   return 0 ;
}

