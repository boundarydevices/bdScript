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
 * Revision 1.1  2003-11-30 16:45:53  ericn
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
#include <linux/mtd/mtd.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

static unsigned const maxKernelSize = ( 1<<20 );
static char const kernelDev[] = {
   "/dev/mtd1"
};

static char const fileSysDev[] = {
   "/dev/mtd2"
};

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
         unsigned const totalSize = numRead ;
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

         unsigned const paramStart = numRead ;
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

printf( "0x%x bytes of kernel partition\n"
        "0x%x bytes of parameter data\n"
        "0x%x bytes of erased data\n"
        "0x%x bytes of boot loader+partition\n",
        totalSize, totalSize-paramStart, paramStart-numRead, numRead );

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

static JSBool
jsKernelUpgrade( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sKernel = JSVAL_TO_STRING( argv[0] );
      char const *kernelData = JS_GetStringBytes( sKernel );
      unsigned const kernelLength = JS_GetStringLength( sKernel );
      if( ( (1<<16) < kernelLength ) && ( '\xEA' == kernelData[3] ) )
      {
         printf( "program %u bytes of kernel here\n", kernelLength );
         int const devFd = open( kernelDev, O_RDWR );
         if( 0 <= devFd )
         {
            mtd_info_t meminfo;
            if( 0 == ioctl( devFd, MEMGETINFO, &meminfo ) )
            {   
               erase_info_t erase;
               erase.start = 0 ;
               erase.length = meminfo.erasesize;
               
               unsigned const numToErase = meminfo.size / meminfo.erasesize ;
               unsigned count = 0 ;
               for( ; count < numToErase ; count++ ) 
               {
                  if( 0 == ioctl( devFd, MEMERASE, &erase ) )
                  {
                     printf( "." ); fflush( stdout );
                  }
                  else
                  {      
                     printf( "\r\nMTD Erase failure");
                     break;
                  }
                  erase.start += meminfo.erasesize;
               }

               unsigned numWritten = write( devFd, kernelData, kernelLength );
               if( ( numWritten == kernelLength ) && ( numToErase == count ) )
               {
                  printf( "%u bytes programmed successfully\n", kernelLength );
                  *rval = JSVAL_TRUE ;
               }
               else
                  JS_ReportError( cx, "erased %u of %u pages, wrote %u out of %u bytes\n", count, numToErase, numWritten, kernelLength );
            }
            else
               JS_ReportError( cx, "%s reading partition info", strerror( errno ) );
            
            close( devFd );
         }
         else
            JS_ReportError( cx, "%s opening %s", strerror( errno ), kernelDev );
      }
      else
         JS_ReportError( cx, "Doesn't look like a kernel image\n" );
   }
   else
      JS_ReportError( cx, "Usage: kernel.upgrade( 'kernelData' (string) )" );

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
   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sFileSys = JSVAL_TO_STRING( argv[0] );
      char const *fileSysData = JS_GetStringBytes( sFileSys );
      unsigned const fileSysLength = JS_GetStringLength( sFileSys );
      if( (1<<20) < fileSysLength )
      {
         printf( "program %u bytes of fileSys here\n", fileSysLength );
         int umRes = umount( "/" );
         if( 0 == umRes )
         {
            int const devFd = open( fileSysDev, O_RDWR );
            if( 0 <= devFd )
            {
               mtd_info_t meminfo;
               if( 0 == ioctl( devFd, MEMGETINFO, &meminfo ) )
               {   
                  erase_info_t erase;
                  erase.start = 0 ;
                  erase.length = meminfo.erasesize;
                  
                  unsigned const numToErase = meminfo.size / meminfo.erasesize ;
                  unsigned count = 0 ;
                  for( ; count < numToErase ; count++ ) 
                  {
                     if( 0 == ioctl( devFd, MEMERASE, &erase ) )
                     {
                        printf( "." ); fflush( stdout );
                     }
                     else
                     {      
                        printf( "\r\nMTD Erase failure");
                        break;
                     }
                     erase.start += meminfo.erasesize;
                  }
   
                  unsigned numWritten = write( devFd, fileSysData, fileSysLength );
                  if( ( numWritten == fileSysLength ) && ( numToErase == count ) )
                  {
                     printf( "%u bytes programmed successfully\n", fileSysLength );
                     *rval = JSVAL_TRUE ;
                  }
                  else
                     JS_ReportError( cx, "erased %u of %u pages, wrote %u out of %u bytes\n", count, numToErase, numWritten, fileSysLength );
               }
               else
                  JS_ReportError( cx, "%s reading partition info", strerror( errno ) );
               
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
      JS_ReportError( cx, "Usage: fileSys.upgrade( 'fileSysData' (string) )" );

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

