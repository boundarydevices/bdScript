/*
 * Module cbmGraph.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: cbmReset.cpp,v $
 * Revision 1.1  2003-05-24 15:23:43  ericn
 * -Initial import
 *
 * Revision 1.2  2003/05/10 03:18:27  ericn
 * -modified to USB status constants (not Posix)
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

/* ioctls: */
#define LPGETSTATUS		0x060b		/* same as in drivers/char/lp.c */
#define IOCNR_GET_DEVICE_ID		1
#define IOCNR_GET_PROTOCOLS		2
#define IOCNR_SET_PROTOCOL		3
#define IOCNR_HP_SET_CHANNEL		4
#define IOCNR_GET_BUS_ADDRESS		5
#define IOCNR_GET_VID_PID		6
#define IOCNR_RESET		7
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
/* Soft reset to USB printer (LP) controller */
#define LPIOC_RESET _IOC(_IOC_WRITE, 'P', IOCNR_RESET, 0)

#define MAXDEVICE 1024

#define USBLP_PAPEROUT  '\x20'
#define USBLP_SELECT    '\x10'
#define USBLP_NOERROR   '\x08'

#define CBM_OFFLINE '\x08'

int main( void )
{
   printf( "Hello, world\n" );
   int const fd = open( "/dev/usb/lp0", O_RDWR );
   if( 0 <= fd )
   {
      printf( "printer port opened\n" );
      int err = ioctl( fd, LPIOC_RESET, 0 );
      if( 0 == err )
         printf( "device reset successfully\n" );
      else
         fprintf( stderr, "Error %m resetting device\n" );
      
      printf( "closing port\n" );
      close( fd );
   }
   else
      perror( "/dev/usb/lp0" );
   
   return 0 ;
}

