/*
 * Module jsPrinter.cpp
 *
 * This module defines the Javascript interface routines
 * for the printer class as described in jsPrinter.h
 *
 *
 * Change History : 
 *
 * $Log: jsPrinter.cpp,v $
 * Revision 1.4  2004-05-12 03:46:17  ericn
 * -added read() method
 *
 * Revision 1.3  2004/05/12 03:42:26  ericn
 * -added read() method
 *
 * Revision 1.2  2004/05/08 16:33:46  ericn
 * -removed debug msg
 *
 * Revision 1.1  2004/05/05 03:20:32  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "jsPrinter.h"
#include "jsCBM.h"
#include "jsStar.h"

#include <string.h>
#include "js/jsstddef.h"
#include "js/jscntxt.h"
#include "js/jsapi.h"
#include "js/jslock.h"
#include "jsAlphaMap.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <linux/lp.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>

/* ioctls: */
#define LPGETSTATUS		0x060b		/* same as in drivers/char/lp.c */
#define IOCNR_GET_DEVICE_ID		1
#define IOCNR_GET_PROTOCOLS		2
#define IOCNR_SET_PROTOCOL		3
#define IOCNR_HP_SET_CHANNEL		4
#define IOCNR_GET_BUS_ADDRESS		5
#define IOCNR_GET_VID_PID		6
#define IOCNR_RESET		        7
#define IOCNR_1284STAT		        8
#define IOCNR_GETCTRLREG   	        9
#define IOCNR_CHIPVER		        10
#define IOCNR_GETSTATUS		        11

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
#define LPIOC_RESET            _IOC(_IOC_WRITE, 'P', IOCNR_RESET, 0)
#define LPIOC_GET_VID_PID(len) _IOC(_IOC_READ, 'P', IOCNR_GET_VID_PID, len)
#define LPIOC_1284STAT(len)   _IOC(_IOC_READ, 'P', IOCNR_1284STAT, len)
#define LPIOC_GETCTRLREG(len) _IOC(_IOC_READ, 'P', IOCNR_GETCTRLREG, len)
#define LPIOC_CHIPVER(len)    _IOC(_IOC_READ, 'P', IOCNR_CHIPVER, len)
#define LPIOC_GETSTATUS(len)  _IOC(_IOC_READ, 'P', IOCNR_GETSTATUS, len)

#define MAXDEVICE 1024

enum jsPrinter_tinyId {
   PRINTER_DEVICEID,
};

static void jsPrinterFinalize(JSContext *cx, JSObject *obj);

JSClass jsPrinterClass_ = {
  "printer",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsPrinterFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec printerProperties_[] = {
  {"deviceId",        PRINTER_DEVICEID,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static unsigned const maxWidthBits  = 432 ;
static unsigned const maxWidthBytes = maxWidthBits/8 ;
static unsigned const maxN1xN2      = 1536 ;
JSObject *printerProto = NULL ;

static JSBool
jsPrinterPrint( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   JSObject *rhObj ;
   JSString *sArg = 0 ;
   int const *pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsPrinterClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      if( ( 1 == argc )
          &&
          JSVAL_IS_OBJECT( argv[0] ) 
          &&
          ( 0 != ( rhObj = JSVAL_TO_OBJECT( argv[0] ) ) ) )
      {
         if( JS_CallFunctionName( cx, obj, "imageToString", argc, argv, rval )
             &&
             JSVAL_IS_STRING( *rval )
             &&
             ( 0 != ( sArg = JSVAL_TO_STRING( *rval ) ) ) )
         {
         }
         else
            JS_ReportError( cx, "converting image to string\n" );
      }
      else if( ( 1 == argc )
               &&
               JSVAL_IS_STRING( argv[0] )
               &&
               ( 0 != ( sArg = JSVAL_TO_STRING( argv[0] ) ) ) )
      {
      }
      else
      {
         JS_ReportError( cx, "Usage: printer.print( alphaMap|string )" );
         sArg = 0 ; // just in case
      }

      if( 0 != sArg )
      {
         unsigned const outLen = JS_GetStringLength( sArg );
         int const numWritten = write( *pfd, JS_GetStringBytes( sArg ), outLen );
         if( numWritten != outLen )
            printf( "wrote %d of %u string bytes\n", numWritten, outLen );
      }
   }
   else
      JS_ReportError( cx, "Invalid printer fd\n" );
   
   return JS_TRUE ;
}

static JSBool
jsPrinterFlush( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsPrinterClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      struct pollfd ufds;
      ufds.fd = *pfd;
      ufds.events = POLLOUT;
      ufds.revents = 0;
      if (poll( &ufds, 1, 5000 )) *rval = JSVAL_TRUE ; //true if no timeout
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   
   return JS_TRUE ;
}

static JSBool
jsPrinterClose( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsPrinterClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
fprintf( stderr, "---> closing printer:fd %d\n", *pfd );
      close( *pfd );
      *pfd = -1 ;
      JS_SetPrivate( cx, obj, pfd );

      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   
   return JS_TRUE ;
}

static JSBool
jsPrinterRead( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsPrinterClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      struct pollfd ufds;
      ufds.fd = *pfd;
      ufds.events = POLLIN;
      ufds.revents = 0;
      if( 0 != poll( &ufds, 1, 0 ) )
      {
         char inBuf[256];
         int numRead = read( *pfd, inBuf, sizeof( inBuf ) );
         if( 0 <= numRead )
         {
            JSString *sOut = JS_NewStringCopyN( cx, inBuf, numRead );
            *rval = STRING_TO_JSVAL( sOut );
         }
         else
            perror( "printerRead" );
      } // data present
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   return JS_TRUE ;
}

static JSFunctionSpec printer_methods[] = {
   { "print",        jsPrinterPrint,      0,0,0 },
   { "close",        jsPrinterClose,      0,0,0 },
   { "flush",        jsPrinterFlush,      0,0,0 },
   { "read",         jsPrinterRead,       0,0,0 },
   { 0 }
};

static void jsPrinterFinalize(JSContext *cx, JSObject *obj)
{
   if( obj )
   {
      int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsPrinterClass_, NULL );
      if( ( 0 != pfd ) && ( 0 <= *pfd ) )
      {
fprintf( stderr, "---> closing printer:fd %d\n", *pfd );
         close( *pfd );
         *pfd = -1 ;
         JS_SetPrivate( cx, obj, pfd );
      } // have button data
   }
}

static JSBool jsPrinter( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSObject *prObj = JS_NewObject( cx, &jsPrinterClass_, printerProto, obj );
      if( prObj )
      {
         *rval = OBJECT_TO_JSVAL(prObj); // root
         JSString *sDevice = JSVAL_TO_STRING( argv[0] );
         int *pfd = (int *)JS_malloc( cx, sizeof(*pfd) );
         *pfd = -1 ;
         JS_SetPrivate( cx, prObj, pfd );
         int const fd = open( JS_GetStringBytes( sDevice ), O_RDWR );
         if( 0 <= fd )
         {
            fcntl( fd, F_SETFD, FD_CLOEXEC );
            *pfd = fd ;

            char * const device_id = new char[MAXDEVICE];	// Device ID string

            if (ioctl(fd, LPIOC_GET_DEVICE_ID(MAXDEVICE), device_id) == 0)
            {
               unsigned length = (((unsigned)device_id[0] & 255) << 8) +
                                 ((unsigned)device_id[1] & 255);
               memcpy(device_id, device_id + 2, length);
               device_id[length] = '\0';
               printf( "device id:%s\n", device_id );
               JSString *sDeviceId = JS_NewStringCopyN( cx, (char *)device_id, length );
               if( sDeviceId )
               {
                  JS_DefineProperty( cx, prObj, "deviceId", 
                                     STRING_TO_JSVAL( sDeviceId ),
                                     0, 0, 
                                     JSPROP_ENUMERATE
                                     |JSPROP_PERMANENT
                                     |JSPROP_READONLY );
                  if( 0 != strstr( device_id, "CBM" ) )
                  {
                     if( JS_DefineFunctions( cx, prObj, cbm_methods ) )
                        CBMPrinterFixup( cx, prObj );
                     else 
                        JS_ReportError( cx, "adding CBM methods\n" );
                  }
                  else if( 0 != strstr( device_id, "STAR" ) )
                  {
                     if( JS_DefineFunctions( cx, prObj, star_methods ) )
                     {
                        starPrinterFixup( cx, prObj );
                     }
                     else
                        JS_ReportError( cx, "adding Star methods\n" );
                  }
               }
               else
                  JS_ReportError( cx, "Error allocating deviceId" );
            }
            else
               perror( "getDeviceId" );

            delete [] device_id ;

         }
         else
            JS_ReportError( cx, "%s opening printer fd", strerror( errno ) );
      }
      else
         JS_ReportError( cx, "Error allocating printer" );
   }
   else
      JS_ReportError( cx, "Usage: var printer = new printer( '/dev/usb/lp0' );" );

   return JS_TRUE;
}

#include <unistd.h>

bool initPrinter( JSContext *cx, JSObject *glob )
{
   if( 0 == printerProto )
   {
      printerProto = JS_InitClass( cx, glob, NULL, &jsPrinterClass_,
                                   jsPrinter, 1,
                                   printerProperties_, 
                                   printer_methods,
                                   0, 0 );
      if( printerProto )
      {
         JS_AddRoot( cx, &printerProto );
      }
      else
         JS_ReportError( cx, "initializing CBM printer class\n" );
   }
   
   return ( 0 != printerProto );
}

