/*
 * Module cbmGraph.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: cbmStat.cpp,v $
 * Revision 1.3  2005-08-22 13:11:41  ericn
 * -string.h
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
#include <string.h>

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

#define USBLP_PAPEROUT  '\x20'
#define USBLP_SELECT    '\x10'
#define USBLP_NOERROR   '\x08'

int main( void )
{
   printf( "Hello, world\n" );
   int const fd = open( "/dev/usb/lp0", O_WRONLY );
   if( 0 <= fd )
   {
      printf( "printer port opened\n" );

      char * const device_id = new char[MAXDEVICE];	// Device ID string

      if (ioctl(fd, LPIOC_GET_DEVICE_ID(MAXDEVICE), device_id) == 0)
      {
         unsigned length = (((unsigned)device_id[0] & 255) << 8) +
                           ((unsigned)device_id[1] & 255);
         memcpy(device_id, device_id + 2, length);
         device_id[length] = '\0';
         printf( "device id:%s\n", device_id );
      }
      else
         perror( "getDeviceId" );

      int twoints[2];
      if (ioctl(fd, LPIOC_GET_PROTOCOLS(sizeof(twoints)), twoints) == 0)
      {
         printf( "protocols :%08X:%08X\n", twoints[0], twoints[1] );
      }
      else
         perror( "getProtocols" );
      
      if (ioctl(fd, LPIOC_GET_BUS_ADDRESS(sizeof(twoints)), twoints) == 0)
      {
         printf( "bus address :%08X:%08X\n", twoints[0], twoints[1] );
      }
      else
         perror( "get bus address" );

      if (ioctl(fd, LPIOC_GET_VID_PID(sizeof(twoints)), twoints) == 0)
      {
         printf( "vid %08X, pid %08X\n", twoints[0], twoints[1] );
      }
      else
         perror( "get vid pid" );

      unsigned char status ;
      if( ioctl(fd, LPGETSTATUS, &status) == 0) 
      { 
         fprintf(stderr, "DEBUG: LPGETSTATUS returned a port status of %02X...\n", status); 
         if (status & USBLP_PAPEROUT) 
            fputs("WARNING: Paper Out!\n", stderr); 
         if (0 == (status & USBLP_SELECT) )
            fputs("WARNING: Printer is off-line.\n", stderr); 
         if (0 == (status & USBLP_NOERROR) )
            fputs("WARNING: Printer Fault.\n", stderr); 
      } 
      
      printf( "closing port\n" );
      close( fd );
   }
   else
      perror( "/dev/usb/lp0" );
   
   return 0 ;
}

