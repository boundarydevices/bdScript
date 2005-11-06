/*
 * Module jsCBM.cpp
 *
 * This module defines the initialization and Javascript interface
 * routines for the CBM printer.
 *
 *
 * Change History : 
 *
 * $Log: jsCBM.cpp,v $
 * Revision 1.17  2005-11-06 00:49:27  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.16  2004/05/05 03:18:34  ericn
 * -updated to work with jsPrinter base class
 *
 * Revision 1.15  2004/03/27 20:23:10  ericn
 * -added ioctl calls
 *
 * Revision 1.14  2003/07/08 13:15:17  ericn
 * -added CLOEXEC to printer file handle
 *
 * Revision 1.13  2003/06/29 17:36:54  tkisky
 * -debug code
 *
 * Revision 1.12  2003/06/26 08:02:41  tkisky
 * -add error message data
 *
 * Revision 1.11  2003/06/22 23:04:49  ericn
 * -modified to use constructor for initialization, private data for fd
 *
 * Revision 1.10  2003/06/06 01:48:46  ericn
 * -added include
 *
 * Revision 1.9  2003/06/05 14:35:17  ericn
 * -modified error messages to use strerror
 *
 * Revision 1.8  2003/06/04 14:38:33  ericn
 * -added deviceId member
 *
 * Revision 1.7  2003/06/04 02:56:45  ericn
 * -modified to allocate printer object even if unplugged
 *
 * Revision 1.6  2003/05/26 22:06:15  ericn
 * -modified to use cbmImage
 *
 * Revision 1.5  2003/05/24 15:03:35  ericn
 * -updated to slice images to fit printer RAM
 *
 * Revision 1.4  2003/05/18 21:52:26  ericn
 * -added print( string ) method
 *
 * Revision 1.3  2003/05/10 19:16:43  ericn
 * -added cut command, modified to keep printer fd
 *
 * Revision 1.2  2003/05/10 03:18:54  ericn
 * -removed redundant test, changed CBM print command
 *
 * Revision 1.1  2003/05/09 04:28:12  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsCBM.h"
#include <string.h>
#include "js/jsstddef.h"
#include "js/jscntxt.h"
#include "js/jsapi.h"
#include "js/jslock.h"
#include "jsAlphaMap.h"
#include "cbmImage.h"
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

enum jsCBM_tinyId {
   CBM_DEVICEID,
};

static void jsCBMFinalize(JSContext *cx, JSObject *obj);

JSClass jsCBMClass_ = {
  "CBM",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsCBMFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec cbmProperties_[] = {
  {"deviceId",        CBM_DEVICEID,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static unsigned const maxWidthBits  = 432 ;
static unsigned const maxWidthBytes = maxWidthBits/8 ;
static unsigned const maxN1xN2      = 1536 ;


static JSBool
jsCBMPrint( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   JSObject *rhObj ;
   JSString *sArg ;
   int const *pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      if( ( 1 == argc )
          &&
          JSVAL_IS_OBJECT( argv[0] ) 
          &&
          ( 0 != ( rhObj = JSVAL_TO_OBJECT( argv[0] ) ) ) )
      {
         jsval     vPixMap ;
         jsval     vWidth ;
         jsval     vHeight ;
         JSString *sPixMap ;
      
         if( JS_GetProperty( cx, rhObj, "pixBuf", &vPixMap )
             &&
             JSVAL_IS_STRING( vPixMap )
             &&
             ( 0 != ( sPixMap = JSVAL_TO_STRING( vPixMap ) ) )
             &&
             JS_GetProperty( cx, rhObj, "width", &vWidth )
             &&
             JSVAL_IS_INT( vWidth )
             &&
             JS_GetProperty( cx, rhObj, "height", &vHeight )
             &&
             JSVAL_IS_INT( vHeight ) )
         {
            unsigned const bmWidth    = JSVAL_TO_INT( vWidth );
            unsigned       bmHeight   = JSVAL_TO_INT( vHeight );
            if( JS_GetStringLength( sPixMap ) == bmWidth*bmHeight )
            {
               unsigned char const *alphaData = (unsigned char const *)JS_GetStringBytes( sPixMap );
   
               cbmImage_t image( bmWidth, bmHeight );
               for( unsigned y = 0 ; y < bmHeight ; y++ )
               {
                  for( unsigned x = 0 ; x < bmWidth; x++ )
                  {
                     if( 0 == *alphaData++ )
                        image.setPixel( x, y );
                  }
               }
               const void *const p = image.getData();
               unsigned len = image.getLength();
#if 1	//test code
	       {
			unsigned char* data = (unsigned char*)p;
			int l = len;
			printf("%i bytes to be printed\n",l);
#if 1
			int const fd = open( "/tmp/testNew.prt", O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR );
			if (fd>=0) {
//				int const numWritten = 
            write( fd, data, l );
				close(fd);
			}
			else printf("error opening /tmp/testNew.prt\n");
#endif			
			while (l>4) {
				int c1,c2,c3,c4;
				int size;
				c1 = *data++;
				c2 = *data++;
				c3 = *data++;
				c4 = *data++;
				l -= 4;
				size = (c3*c4*8);
				printf("%2x %2x %2x %2x header, data size %i\n",c1,c2,c3,c4,size);
#if 0
				while (size) {
					int i = *data++;
					printf("%2x",i);
					size--;
				}
#else
				data += size;
#endif
				l -= size;
				if (l >= 3) {
					c1 = *data++;
					c2 = *data++;
					c3 = *data++;
					l -= 3;
					printf("%2x %2x %2x trail\n",c1,c2,c3);
				}
			}
			if (l!=0) printf("%i extra bytes",l);
		}
#endif
               int const numWritten = write( *pfd, p, len );
               if( (unsigned)numWritten == image.getLength() )
               {
                  *rval = JSVAL_TRUE ;
               }
               else
                  JS_ReportError( cx, "%s(%i) sending print data, %p address, %i length, %i ret", strerror( errno ),errno, p, len,numWritten);
            }
            else
               JS_ReportError( cx, "Invalid pixMap" );
         }
         else
            JS_ReportError( cx, "Error retrieving alphaMap fields" );
      }
      else if( ( 1 == argc )
               &&
               JSVAL_IS_STRING( argv[0] )
               &&
               ( 0 != ( sArg = JSVAL_TO_STRING( argv[0] ) ) ) )
      {
         unsigned const outLen = JS_GetStringLength( sArg );
         int const numWritten = write( *pfd, JS_GetStringBytes( sArg ), outLen );
         if( (unsigned)numWritten != outLen )
            printf( "wrote %d of %u string bytes\n", numWritten, outLen );
   
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "Usage: printer.print( alphaMap|string )" );
   }
   else
      JS_ReportError( cx, "Invalid printer fd\n" );
   
   return JS_TRUE ;
}

static JSBool
jsCBMCut( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      write( *pfd, "\n\n\x1dV", 5 );
      
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   
   return JS_TRUE ;
}

static JSBool
jsCBMFlush( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
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
jsCBMClose( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
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
jsCBMReset( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      if( 0 == ioctl(*pfd, LPIOC_RESET, 0) ) 
      {
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "%s resetting port", strerror( errno ) );
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   return JS_TRUE ;
}

static JSBool
jsCBMGetStatus( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      unsigned char status = 0x5a ;
      if( 0 == ioctl(*pfd, LPIOC_GETSTATUS(sizeof(status)), &status) ) 
      {
         *rval = INT_TO_JSVAL( status );
      }
      else
         JS_ReportError( cx, "%s reading port status", strerror( errno ) );
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   return JS_TRUE ;
}

static JSBool
jsCBMChipVer( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      unsigned short chipVer = 0x0123 ;
      if( 0 == ioctl(*pfd, LPIOC_CHIPVER(sizeof(chipVer)), &chipVer) ) 
      {
         *rval = INT_TO_JSVAL( chipVer );
      }
      else
         JS_ReportError( cx, "%s reading chip version", strerror( errno ) );
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   return JS_TRUE ;
}

static JSBool
jsCBMCtrlReg( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      unsigned long ctrlReg = 0x5555aaaa ;
      if( 0 == ioctl(*pfd, LPIOC_GETCTRLREG(sizeof(ctrlReg)), &ctrlReg) ) 
      {
         *rval = INT_TO_JSVAL( ctrlReg );
      }
      else
         JS_ReportError( cx, "%s reading ctrl reg", strerror( errno ) );
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   return JS_TRUE ;
}

static JSBool
jsCBM1284Stat( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
   if( ( 0 != pfd ) && ( 0 <= *pfd ) )
   {
      unsigned long ieeStat = 0xaaaa5555 ;
      if( 0 == ioctl(*pfd, LPIOC_1284STAT(sizeof(ieeStat)), &ieeStat) ) 
      {
         *rval = INT_TO_JSVAL( ieeStat );
      }
      else
         JS_ReportError( cx, "%s reading 1284 status", strerror( errno ) );
   }
   else
      JS_ReportError( cx, "Invalid printer handle" );
   return JS_TRUE ;
}


JSFunctionSpec cbm_methods[10] = {
   { "imageToString",   jsCBMPrint,      0,0,0 },
   { "cut",             jsCBMCut,        0,0,0 },
   { "close",           jsCBMClose,      0,0,0 },
   { "flush",           jsCBMFlush,      0,0,0 },
   { "reset",           jsCBMReset,      0,0,0 },
   { "getStatus",       jsCBMGetStatus,  0,0,0 },
   { "chipVer",         jsCBMChipVer,    0,0,0 },
   { "ctrlReg",         jsCBMCtrlReg,    0,0,0 },
   { "ieeeStat",        jsCBM1284Stat,   0,0,0 },
   { 0 }
};

static void jsCBMFinalize(JSContext *cx, JSObject *obj)
{
   if( obj )
   {
      int *const pfd = (int *)JS_GetInstancePrivate( cx, obj, &jsCBMClass_, NULL );
      if( ( 0 != pfd ) && ( 0 <= *pfd ) )
      {
fprintf( stderr, "---> closing printer:fd %d\n", *pfd );
         close( *pfd );
         *pfd = -1 ;
         JS_SetPrivate( cx, obj, pfd );
      } // have button data
   }
}

static JSBool jsCBM( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSObject *obj = JS_NewObject( cx, &jsCBMClass_, NULL, NULL );
      if( obj )
      {
         *rval = OBJECT_TO_JSVAL(obj); // root
         JSString *sDevice = JSVAL_TO_STRING( argv[0] );
         int *pfd = (int *)JS_malloc( cx, sizeof(*pfd) );
         *pfd = -1 ;
         JS_SetPrivate( cx, obj, pfd );
         int const fd = open( JS_GetStringBytes( sDevice ), O_WRONLY );
         if( 0 <= fd )
         {
            fcntl( fd, F_SETFD, FD_CLOEXEC );
fprintf( stderr, "-----> opened printer: fd == %d\n", fd );
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
                  JS_DefineProperty( cx, obj, "deviceId", 
                                     STRING_TO_JSVAL( sDeviceId ),
                                     0, 0, 
                                     JSPROP_ENUMERATE
                                     |JSPROP_PERMANENT
                                     |JSPROP_READONLY );
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
         JS_ReportError( cx, "Error allocating CBM printer" );
   }
   else
      JS_ReportError( cx, "Usage: var printer = new CBM( '/dev/usb/lp0' );" );

   return JS_TRUE;
}

#include <unistd.h>

bool initJSCBM( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsCBMClass_,
                                  jsCBM, 1,
                                  cbmProperties_, 
                                  cbm_methods,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   else
      JS_ReportError( cx, "initializing CBM printer class\n" );
   
   return false ;
}

void CBMPrinterFixup( JSContext *cx, 
                      JSObject  *obj )
{
   JS_DefineProperty( cx, obj, "width", 
                      INT_TO_JSVAL( maxWidthBits ),
                      0, 0, 
                      JSPROP_ENUMERATE
                      |JSPROP_PERMANENT
                      |JSPROP_READONLY );
}

