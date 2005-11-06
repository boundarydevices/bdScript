/*
 * Module jsKernel.cpp
 *
 * This module defines the kernel object as 
 * declared in jsKernel.h
 *
 *
 * Change History : 
 *
 * $Log: jsKernel.cpp,v $
 * Revision 1.6  2005-11-06 00:49:33  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.5  2005/08/12 04:19:36  ericn
 * -allow compile against 2.6
 *
 * Revision 1.4  2004/09/28 04:02:59  ericn
 * -don't draw to screen for BD2004, reboot when done programming
 *
 * Revision 1.3  2003/12/04 02:23:45  ericn
 * -more error messages
 *
 * Revision 1.2  2003/12/01 04:54:58  ericn
 * -adde progress bar
 *
 * Revision 1.1  2003/11/30 16:45:53  ericn
 * -kernel and JFFS2 upgrade support
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsKernel.h"
#include "openssl/md5.h"
#include "md5.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifndef KERNEL_2_6
#include <linux/mtd/mtd.h>
#else
#include <mtd/mtd-user.h>
#endif
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <signal.h>
#include "fbDev.h"
#include "meter.h"

static unsigned const maxKernelSize = ( 1<<20 );
static char const kernelDev[] = {
   "/dev/mtd1"
};

static char const fileSysDev[] = {
#ifdef CONFIG_BD2003
   "/dev/mtd2"
#else 
   "/dev/mtd3"
#endif
};

#ifdef CONFIG_BD2003
#define FBRECT( xl, yt, xr, yb, r, g, b ) fb.rect( xl, yt, xr, yb, r, g, b )
#else
#define FBRECT( xl, yt, xr, yb, r, g, b ) /**/
#endif 

static JSBool
jsKernelMD5( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   unsigned char *const kernelData = new unsigned char[ maxKernelSize ];
   int fdKernel = open( kernelDev, O_RDONLY );
   if( 0 <= fdKernel )
   {
      unsigned numRead = read( fdKernel, kernelData, maxKernelSize );
      if( 0 <= numRead )
      {
         //
         // kernel partition consists of 
         //    boot loader + kernel
         //    1 or more bytes of ff's (erased area)
         //    0 or more lines of parameter data
         //

         //
         // find the start of the parameter data
         //
         while( 0 < numRead )
         {
            --numRead ;
            if( '\xff' == kernelData[numRead] )
            {
               ++numRead ;
               break;
            }
         }

         while( 0 < numRead )
         {
            --numRead ;
            if( '\xff' == kernelData[numRead] )
               ; // still in erased area
            else
            {
               ++numRead ;
               break;
            }
         }

/*
printf( "0x%x bytes of kernel partition\n"
        "0x%x bytes of parameter data\n"
        "0x%x bytes of erased data\n"
        "0x%x bytes of boot loader+partition\n",
        totalSize, totalSize-paramStart, paramStart-numRead, numRead );
*/
         if( 0 < numRead )
         {
            md5_t result ;
            MD5_CTX ctx ;
            MD5_Init( &ctx );
            MD5_Update( &ctx, kernelData, numRead );
            MD5_Final( result.md5bytes_, &ctx );
   
            unsigned const nalloc = sizeof( result.md5bytes_)*2+1 ;
            char * const stringMem = (char *)JS_malloc( cx, nalloc );
            char *nextOut = stringMem ;
            for( unsigned i = 0 ; i < sizeof( result.md5bytes_); i++ )
               nextOut += sprintf( nextOut, "%02x", result.md5bytes_[i] );
            *nextOut = '\0' ;
         
            *rval = STRING_TO_JSVAL( JS_NewString( cx, stringMem, nalloc - 1 ) );
         }
         else
            JS_ReportError( cx, "Kernel is clear!\n" );
      }
      else
         JS_ReportError( cx, "reading kernel data" );
      close( fdKernel );
   }
   else
      JS_ReportError( cx, "opening kernel device" );

   delete [] kernelData ;

   return JS_TRUE ;
}

//
// both kernel.upgrade() and fileSys.upgrade() accept
// four optional parameters which control the output of 
// a horizontal progress bar.
//

static bool programParams( JSContext   *cx, 
                           uintN        argc, 
                           jsval const  argv[],
                           char const *&programData,
                           unsigned    &programLength,
                           unsigned    &startX,
                           unsigned    &endX,
                           unsigned    &topY,
                           unsigned    &height )
{
   startX = endX = topY = height = 0 ;
   
   if( ( 1 <= argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sKernel = JSVAL_TO_STRING( argv[0] );
      programData   = JS_GetStringBytes( sKernel );
      programLength = JS_GetStringLength( sKernel );

      if( ( 5 == argc )
          &&
          JSVAL_IS_INT( argv[1] )
          &&
          JSVAL_IS_INT( argv[2] )
          &&
          JSVAL_IS_INT( argv[3] )
          &&
          JSVAL_IS_INT( argv[4] ) )
      {
         startX = JSVAL_TO_INT( argv[1] );
         endX   = JSVAL_TO_INT( argv[2] );
         topY   = JSVAL_TO_INT( argv[3] );
         height = JSVAL_TO_INT( argv[4] );
         fbDevice_t &fb = getFB();

         if( ( endX > startX )
             &&
             ( endX < fb.getWidth() )
             &&
             ( 0 < height )
             &&
             ( startX + height < fb.getHeight() ) )
         {
         } // parameters are valid
         else
         {
            startX = endX = topY = height = 0 ;
         }
      } // have progress bar parameters
      else
      {
         printf( "No progress bar parameters\n" );
      } // no optional parameters

      return true ;
   }
   else
      printf( "invalid parameter count %u or param0 type %u\n", argc, JS_TypeOfValue( cx, argv[0] ) );
   
   return false ;
}

static void doReboot()
{
   signal(SIGTERM,SIG_IGN);
   signal(SIGHUP,SIG_IGN);
   setpgrp();
   
   /* Allow Ctrl-Alt-Del to reboot system. */
   reboot(RB_ENABLE_CAD);
   
   /* Send signals to every process _except_ pid 1 */
   kill(-1, SIGTERM);
   sleep(1);
   
   kill(-1, SIGKILL);
   sleep(1);
   
   sync();
   
   reboot(RB_AUTOBOOT);
   kill(1, SIGTERM);
   exit(0);
}

static bool doProgram( JSContext  *cx, 
                       int         devFd,
                       void const *data,
                       unsigned    dataLength,
                       unsigned    xLeft, 
                       unsigned    xRight,
                       unsigned    yTop, 
                       unsigned    height )
{
   mtd_info_t meminfo;
   if( 0 == ioctl( devFd, MEMGETINFO, &meminfo ) )
   {   
      erase_info_t erase;
      erase.start = 0 ;
      erase.length = meminfo.erasesize;

      fbDevice_t &fb = getFB();

      unsigned const yBottom = yTop + height - 1 ;
      meter_t meter( 0, meminfo.size,
                     xLeft, xRight==xLeft ? xLeft+1 : xRight );

      unsigned char red   = 0xFF ;
      unsigned char green = 0xFF ;
      unsigned char blue  = 0xFF ;

      if( 0 < height )
         FBRECT( xLeft, yTop, xRight, yBottom, red, green, blue );

      red = green = blue = 0x40 ;

      unsigned const numToErase = meminfo.size / meminfo.erasesize ;
      unsigned count = 0 ;
      unsigned bytePos = 0 ;
      unsigned xNext = xLeft ;
      for( ; count < numToErase ; count++ ) 
      {
         if( 0 == ioctl( devFd, MEMERASE, &erase ) )
         {
            bytePos += meminfo.erasesize ;
            unsigned newLeft = meter.project( bytePos );
            if( 0 < height )
               FBRECT( xNext, yTop, newLeft, yBottom, red, green, blue );
            xNext = newLeft ;
            printf( "." ); fflush( stdout );
         }
         else
         {      
            printf( "\r\nMTD Erase failure");
            break;
         }
         erase.start += meminfo.erasesize;
      }

      // fill if necessary
      if( ( xNext < xRight ) && ( count == numToErase ) && ( 0 < height ) )
         FBRECT( xNext, yTop, xRight, yBottom, red, green, blue );

      unsigned char const *nextIn = (unsigned char const *)data ;

      meter_t meter2( 0, dataLength,
                      xLeft, xRight==xLeft ? xLeft+1 : xRight );
      bytePos = 0 ;
      xNext   = xLeft ;
      red = blue = 0 ;
      green   = 0x80 ;
      unsigned failures = 0 ;
      while( 0 < dataLength )
      {
         unsigned numToWrite = dataLength > meminfo.erasesize ? meminfo.erasesize : dataLength ;
         unsigned numWritten = write( devFd, nextIn, numToWrite );
         if( numWritten == numToWrite )
         {
            nextIn     += numWritten ;
            dataLength -= numWritten ;
            bytePos    += numWritten ;
            unsigned newLeft = meter2.project( bytePos );
            if( 0 < height )
               FBRECT( xNext, yTop, newLeft, yBottom, red, green, blue );
            xNext = newLeft ;
            printf( "|" ); fflush( stdout );
         }
         else
         {
            JS_ReportError( cx, "%s writing flash block\n", strerror( errno ) );
            if( 10 > failures++ )
            {
               if( 0 == lseek( devFd, bytePos, SEEK_SET ) )
               {
                  JS_ReportError( cx, "re-try\n" );
               }
               else
               {
                  JS_ReportError( cx, "%s seeking flash position", strerror( errno ) );
                  break;
               }
            }
            else
            {
               JS_ReportError( cx, "too many retries, giving up\n" );
               break;
            }
         }
      }
      
      if( ( xNext < xRight ) && ( 0 == dataLength ) && ( 0 < height ) )
         FBRECT( xNext, yTop, xRight, yBottom, red, green, blue );

      if( 0 == dataLength )
         doReboot();
   }
   else
      JS_ReportError( cx, "%s reading partition info", strerror( errno ) );

   return false ;

}

static JSBool
jsKernelUpgrade( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   unsigned    xLeft, xRight ;
   unsigned    yTop, height ;
   char const *kernelData ;
   unsigned    kernelLength ;
   if( programParams( cx, argc, argv, kernelData, kernelLength, xLeft, xRight, yTop, height ) )
   {
      if( ( (1<<16) < kernelLength ) && ( '\xEA' == kernelData[3] ) )
      {
         printf( "program %u bytes of kernel here\n", kernelLength );
         int const devFd = open( kernelDev, O_RDWR );
         if( 0 <= devFd )
         {
            if( doProgram( cx, devFd, kernelData, kernelLength, xLeft, xRight, yTop, height ) )
               *rval = JSVAL_TRUE ;
            
            close( devFd );
         }
         else
            JS_ReportError( cx, "%s opening %s", strerror( errno ), kernelDev );
      }
      else
         JS_ReportError( cx, "Doesn't look like a kernel image\n" );
   }
   else
      JS_ReportError( cx, "Usage: kernel.upgrade( 'kernelData' [, xLeft, xRight, yTop, height ] )" );

   return JS_TRUE ;
}

JSClass jsKernelClass_ = {
  "Kernel",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec kernelProperties_[] = {
  {0,0,0}
};

static JSFunctionSpec kernelMethods_[] = {
   { "md5",        jsKernelMD5,      0,0,0 },
   { "upgrade",    jsKernelUpgrade,  0,0,0 },
   { 0 }
};

//
// constructor for the screen object
//
static JSBool jsKernel( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   obj = JS_NewObject( cx, &jsKernelClass_, NULL, NULL );

   if( obj )
   {
      *rval = OBJECT_TO_JSVAL(obj);
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

static JSBool
jsFileSysUpgrade( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   unsigned    xLeft, xRight ;
   unsigned    yTop, height ;
   char const *fileSysData ;
   unsigned    fileSysLength ;
   if( programParams( cx, argc, argv, fileSysData, fileSysLength, xLeft, xRight, yTop, height ) )
   {
      if( (1<<20) < fileSysLength )
      {
         printf( "program %u bytes of fileSys here\n", fileSysLength );
         int umRes = umount( "/" );
         if( 0 == umRes )
         {
            int const devFd = open( fileSysDev, O_RDWR );
            if( 0 <= devFd )
            {
               if( doProgram( cx, devFd, fileSysData, fileSysLength, xLeft, xRight, yTop, height ) )
                  *rval = JSVAL_TRUE ;
               
               close( devFd );
            }
            else
               JS_ReportError( cx, "%s opening %s", strerror( errno ), fileSysDev );
         }
         else
            JS_ReportError( cx, "%d:%s unmounting root\n", umRes, strerror( errno ) );
      }
      else
         JS_ReportError( cx, "Doesn't look like a fileSys image\n" );
   }
   else
      JS_ReportError( cx, "Usage: fileSys.upgrade( 'fileSysData' [, xLeft, xRight, yTop, height ]" );

   return JS_TRUE ;
}

JSClass jsFileSysClass_ = {
  "FileSys",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec fileSysProperties_[] = {
  {0,0,0}
};

static JSFunctionSpec fileSysMethods_[] = {
   { "upgrade",    jsFileSysUpgrade,  0,0,0 },
   { 0 }
};

//
// constructor for the screen object
//
static JSBool jsFileSys( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   obj = JS_NewObject( cx, &jsFileSysClass_, NULL, NULL );

   if( obj )
   {
      *rval = OBJECT_TO_JSVAL(obj);
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

bool initJSKernel( JSContext *cx, JSObject *glob )
{
   JSObject *kernProto = JS_InitClass( cx, glob, NULL, &jsKernelClass_,
                                  jsKernel, 1,
                                  kernelProperties_, 
                                  kernelMethods_,
                                  0, 0 );
   if( kernProto )
   {
      JSObject *obj = JS_NewObject( cx, &jsKernelClass_, kernProto, glob );
      if( obj )
      {
         JS_DefineProperty( cx, glob, "kernel", 
                            OBJECT_TO_JSVAL( obj ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JSObject *fsProto = JS_InitClass( cx, glob, NULL, &jsFileSysClass_,
                                           jsFileSys, 1,
                                           fileSysProperties_, 
                                           fileSysMethods_,
                                           0, 0 );
         if( fsProto )
         {
            JSObject *fsGlob = JS_NewObject( cx, &jsFileSysClass_, fsProto, glob );
            if( fsGlob )
            {
               JS_DefineProperty( cx, glob, "fileSys", 
                                  OBJECT_TO_JSVAL( fsGlob ),
                                  0, 0, 
                                  JSPROP_ENUMERATE
                                  |JSPROP_PERMANENT
                                  |JSPROP_READONLY );
               return true ;
            }
         }
      }
   }
   return false ;
}

