/*
 * Module jsStarUSB.cpp
 *
 * This module defines the implementation of the starUSB
 * class as declared and described in jsStarUSB.h
 *
 *
 * Change History : 
 *
 * $Log: jsStarUSB.cpp,v $
 * Revision 1.1  2004-06-28 02:57:34  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include "jsStarUSB.h"
#include "usb.h"
#include "pollTimer.h"
#include "usb.h"
#include <stdio.h>
#include "dither.h"
#include <assert.h>

#define STAR_VENDOR_ID         0x0519
#define VENDORCLASS_PRODUCT_ID 0x0002

static JSObject *starProto = NULL ;

class starPoll_t : public pollTimer_t {
public:
   starPoll_t( void );
   virtual ~starPoll_t( void );
   
   virtual void fire( void );

   char                   prevData_[512];
   unsigned               prevLen_ ;
   struct usb_dev_handle *udev_ ;
};

static int const inep  = 130; // find_ep(dev, config, interface, altsetting, USB_ENDPOINT_IN, USB_ENDPOINT_TYPE_BULK);
static int const outep = 1 ; // find_ep(dev, config, interface, altsetting, USB_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK);
extern int usb_debug ;

starPoll_t :: starPoll_t( void )
   : pollTimer_t()
   , udev_( 0 )
   , prevLen_( 0 )
{
//   usb_debug = 2 ;
   usb_init();

   usb_find_busses();
   usb_find_devices();
   
   unsigned char found = 0;
   struct usb_bus *bus;
   struct usb_device *dev;
   
   for (bus = usb_busses; bus; bus = bus->next)
   {
      for (dev = bus->devices; dev; dev = dev->next)
      {
         if (dev->descriptor.idVendor == STAR_VENDOR_ID)
         {
            if (dev->descriptor.idProduct == VENDORCLASS_PRODUCT_ID)
            {
               found = 1;
               break;
            }
         }
      }
      
      if( found )
         break;
   }
   
    if( found )
    {
      udev_ = usb_open(dev);
      if( 0 != udev_ )
      {
         int const intNum = dev->config[0].interface[0].altsetting[0].bInterfaceNumber ;
         usb_claim_interface( udev_, intNum );
         set( 100 );
      }
   }
}

starPoll_t :: ~starPoll_t( void )
{
   if( udev_ )
   {
      usb_close( udev_ );
      udev_ = 0 ;
   }
}
   
void starPoll_t :: fire( void )
{
   short availableReadLength = 0;
   if( 0 <= usb_control_msg(udev_, (char) 0xc0, (char) 3, (short) 256, (short) 0, (char *) &availableReadLength, (short) 2, 100 ))
   {
      if( 0 < availableReadLength )
      {
         char inBuf[512];
         int numRead = usb_bulk_read(udev_, inep, inBuf, sizeof(inBuf), 100 );
         if( 0 < numRead )
         {
            if( ( numRead != prevLen_ )
                ||
                ( 0 != memcmp( prevData_, inBuf, prevLen_ ) ) )
                
            {
               for( int i = 0 ; i < numRead ; i++ )
                  printf( "%02x ", (unsigned char)inBuf[i] );
               printf( "\n" );
               memcpy( prevData_, inBuf, numRead );
               prevLen_ = numRead ;
            }
         }
         else
            printf( "read error %d\n", numRead );
      } // data available
   }
   else
      printf( "Error issuing ctrl msg\n" );
   
   fflush( stdout );
   set( 100 );
}

static JSBool jsPrint( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval );

static JSBool jsFlush( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static JSBool jsClose( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static JSBool jsGetStatus( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static JSBool jsOnStatusChange( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static void jsStarUSBFinalize(JSContext *cx, JSObject *obj);

#define DPI          203
#define PAGEHEIGHT   (DPI*2)
#define PAGEWIDTH    832
#define WIDTHBYTES   (PAGEWIDTH/8)

static char const initPrinter[] = {
   "\x1b@"              // initPrinter
};

static char const initRaster[] = {
   "\x1b*rR"            // initialize raster mode (to defaults)
   "\x1b*rA"            // enter raster mode
   "\x1b*rC"            // clear image
};

static char const draftQuality[] = {
   "\x1b*rQ0\x00"       // 0 == high speed, 1 == normal, 2 == letter quality
};

static char const normalQuality[] = {
   "\x1b*rQ0\x01"       // 0 == high speed, 1 == normal, 2 == letter quality
};

static char const letterQuality[] = {
   "\x1b*rQ0\x02"       // 0 == high speed, 1 == normal, 2 == letter quality
};

static char const setPageHeight[] = {
   "\x01b*rP"           // set page height
};

static char const leftMargin[] = {
   "\x1b*rml1\x00"
};

static char const rightMargin[] = {
   "\x1b*rmr1\x00"
};

static char const topMargin[] = {
   "\x1b*rT0\x00"
};

static char const skipLine[] = {
   "\x01b*rY1\0"
};

static char const formFeed[] = {
   "\x1b\x0C\x00" 
};

static char const exitRaster[] = {
   "\x1b*rB"            // exit raster mode
};

static JSBool
jsStarImageToString( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   JSObject *rhObj ;
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
         if( JS_GetStringLength( sPixMap ) == bmWidth*bmHeight*2 )
         {
            unsigned short const *pixData = (unsigned short const *)JS_GetStringBytes( sPixMap );
            char cHeightSpec[12];

            unsigned const rowBytes = (bmWidth+7)/8 ;
            unsigned const imageBytes = bmHeight*(rowBytes+3);    // 'b'+n+m+pixels
            unsigned const totalBytes = + imageBytes ;

            void *const imageData = JS_malloc( cx, totalBytes );

            unsigned char *nextOut = (unsigned char *)imageData ;
/*
printf( "dither..." ); fflush( stdout );
long long const start = tickMs();
*/
            dither_t dither( pixData, bmWidth, bmHeight );
/*
long long const dEnd = tickMs();
printf( "toBits..." ); fflush( stdout );
*/
            for( unsigned y = 0 ; y < bmHeight ; y++ )
            {
               unsigned char * const startOfLine = nextOut ;
               unsigned char * lastDot = startOfLine ;

               *nextOut++ = 'b' ;
               *nextOut++ = '\0' ;
               *nextOut++ = '\0' ;
      
               unsigned char mask = '\x80' ;
               unsigned char out = 0 ;
               for( unsigned x = 0 ; x < bmWidth ; x++ )
               {
                  if( dither.isBlack( x, y ) )
                     out |= mask ;
                  
                  mask >>= 1 ;
                  if( 0 == mask )
                  {
                     mask = '\x80' ;
                     *nextOut++ = out ;
                     if( out )
                        lastDot = nextOut ;
                     out = 0 ;
                  }
               }

               if( ( '\x80' != mask ) && ( 0 != out ) )
               {
                  *nextOut++ = out ;
                  lastDot = nextOut ;
               }

               if( lastDot == startOfLine )
               {

                  memcpy( startOfLine, skipLine, sizeof( skipLine )-1 );
                  nextOut = startOfLine + sizeof( skipLine ) - 1 ;
               } // blank line
               else
               {
                  unsigned const lineBytes = lastDot - startOfLine - 3 ;
                  unsigned char const n = lineBytes / 256;
                  unsigned char const m = lineBytes & 255 ;
                  startOfLine[1] = m ;
                  startOfLine[2] = n ;
//                  nextOut = lastDot ;
               }
            }
/*
long long const rEnd = tickMs();
printf( "done\n"
        "%5llu total, %5llu dither, %5llu render\n",
        rEnd-start, dEnd-start, rEnd-dEnd ); fflush( stdout );
*/
            unsigned const outBytes = nextOut-(unsigned char *)imageData ;
            JSString *sOut = JS_NewString( cx, (char *)imageData, outBytes );
            *rval = STRING_TO_JSVAL( sOut );
// printf( "rendered %u bytes of star image data\n", outBytes );
assert( outBytes <= totalBytes );
         }
         else
            JS_ReportError( cx, "Invalid pixMap" );
      }
      else
         JS_ReportError( cx, "Error retrieving alphaMap fields" );
   }
   else
      JS_ReportError( cx, "Usage: printer.print( 'string' | img )\n" );

   return JS_TRUE ;
}

static JSBool
jsStarPageHeight( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
   {
      char cHeightSpec[80];
      int hSpecLen = sprintf( cHeightSpec, "%s%d", setPageHeight, JSVAL_TO_INT(argv[0]) ) + 1 ;
      JSString *s = JS_NewStringCopyN( cx, cHeightSpec, hSpecLen );
      *rval = STRING_TO_JSVAL( s );
   }
   else
      JS_ReportError( cx, "Usage: printer.pageHeight( #pixels )" );

   return JS_TRUE ;
}

static JSFunctionSpec methods_[] = {
   { "print",           jsPrint,             0,0,0 },
   { "flush",           jsFlush,             0,0,0 },
   { "close",           jsClose,             0,0,0 },
   { "getStatus",       jsGetStatus,         0,0,0 },
   { "onStatusChange",  jsOnStatusChange,    0,0,0 },
   { "imageToString",   jsStarImageToString, 0,0,0 },
   { "pageHeight",      jsStarPageHeight,    0,0,0 },
   { 0 }
};

static JSClass jsStarUSBClass_ = {
  "starUSB",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsStarUSBFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec properties_[] = {
  {0,0,0}
};

static JSBool jsPrint( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   JSObject *rhObj ;
   JSString *sArg = 0 ;
   starPoll_t *const star = (starPoll_t *)JS_GetInstancePrivate( cx, obj, &jsStarUSBClass_, NULL );
   if( ( 0 != star ) && ( 0 != star->udev_ ) )
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
         unsigned    outBytes = JS_GetStringLength( sArg );
         char const *nextOut = JS_GetStringBytes( sArg );
         while( ( 0 < outBytes ) && ( 0 != star->udev_ ) )
         {
            unsigned toWrite = (4096 < outBytes) ? 4096 : outBytes ;
            
            int const numWritten = usb_bulk_write( star->udev_, outep, 
                                                   (char *)nextOut, toWrite, 1000 );
            if( toWrite == numWritten )
            {
               outBytes -= numWritten ;
               nextOut  += numWritten ;
            }
            else if( 0 > numWritten )
            {
               JS_ReportError( cx, "Error writing to printer" );
               usb_close( star->udev_ );
               star->udev_ = 0 ;
            }
            else
            {
               JS_ReportError( cx, "printer stall: %u of %u\n", numWritten, toWrite );
               if( 0 <= usb_clear_halt( star->udev_, outep ) )
               {
                  outBytes -= numWritten ;
                  nextOut  += numWritten ;
               }
               else
               {
                  JS_ReportError( cx, "Error resetting printer" );
                  usb_close( star->udev_ );
                  star->udev_ = 0 ;
               }
            }
         }
      }
   }
   else
      JS_ReportError( cx, "Invalid device handle\n" );
   
   return JS_TRUE ;
}

static void jsStarUSBFinalize(JSContext *cx, JSObject *obj)
{
   starPoll_t *star = (starPoll_t *)JS_GetInstancePrivate( cx, obj, &jsStarUSBClass_, NULL );
   if( star )
   {
      JS_SetPrivate( cx, obj, 0 );
      delete star ;
   }
}

//
// constructor for the screen object
//
static JSBool jsStarUSB( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   JSObject *star = JS_NewObject( cx, &jsStarUSBClass_, starProto, obj );

   if( star )
   {
      *rval = OBJECT_TO_JSVAL(star);
      JS_DefineProperty( cx, star, "width", 
                         INT_TO_JSVAL( PAGEWIDTH ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JSString *s = JS_NewStringCopyN( cx, initPrinter, sizeof( initPrinter ) - 1  );
      JS_DefineProperty( cx, star, "initPrinter", 
                         STRING_TO_JSVAL( s ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      s = JS_NewStringCopyN( cx, initRaster, sizeof( initRaster )-1 );
      JS_DefineProperty( cx, star, "initRaster", 
                         STRING_TO_JSVAL( s ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      s = JS_NewStringCopyN( cx, formFeed, sizeof( formFeed )-1 );
      JS_DefineProperty( cx, star, "formFeed", 
                         STRING_TO_JSVAL( s ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      s = JS_NewStringCopyN( cx, exitRaster, sizeof( exitRaster )-1 );
      JS_DefineProperty( cx, star, "exitRaster", 
                         STRING_TO_JSVAL( s ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      s = JS_NewStringCopyN( cx, draftQuality, sizeof( draftQuality )-1 );
      JS_DefineProperty( cx, star, "draftQuality", 
                         STRING_TO_JSVAL( s ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      s = JS_NewStringCopyN( cx, normalQuality, sizeof( normalQuality )-1 );
      JS_DefineProperty( cx, star, "normalQuality", 
                         STRING_TO_JSVAL( s ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      s = JS_NewStringCopyN( cx, letterQuality, sizeof( letterQuality )-1 );
      JS_DefineProperty( cx, star, "letterQuality", 
                         STRING_TO_JSVAL( s ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      JS_SetPrivate( cx, star, new starPoll_t );
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

bool initJSStarUSB( JSContext *cx, JSObject *glob )
{
   starProto = JS_InitClass( cx, glob, NULL, &jsStarUSBClass_,
                             jsStarUSB, 1,
                             properties_, 
                             methods_,
                             properties_, 0 );
   if( starProto )
   {
      JS_AddRoot( cx, &starProto );
      return true ;
   }
   else
   {
      JS_ReportError( cx, "initializing scanner class" );
   }

   return false ;
}

