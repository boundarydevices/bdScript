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
 * Revision 1.12  2003-06-26 08:02:41  tkisky
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
               int const numWritten = write( *pfd, p, len );
               if( numWritten == image.getLength() )
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
         int const numWritten = write( *pfd, 
                                       JS_GetStringBytes( sArg ), 
                                       JS_GetStringLength( sArg ) );
         printf( "wrote %d string bytes\n", numWritten );
   
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

static JSFunctionSpec cbm_methods[] = {
   { "print",        jsCBMPrint,      0,0,0 },
   { "cut",          jsCBMCut,        0,0,0 },
   { "close",        jsCBMClose,      0,0,0 },
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

