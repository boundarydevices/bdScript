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
 * Revision 1.3  2004-09-10 19:20:15  tkisky
 * -allow ENQ/EOT to be  used for status as well as ASB
 *
 * Revision 1.2  2004/07/04 21:31:27  ericn
 * -added status and bitmap support
 *
 * Revision 1.1  2004/06/28 02:57:34  ericn
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
#include "jsImage.h"
#include "jsAlphaMap.h"
#include "jsBitmap.h"
#include <string.h>
#include "hexDump.h"
#include "jsGlobals.h"
#include "codeQueue.h"

#define STAR_VENDOR_ID         0x0519
#define VENDORCLASS_PRODUCT_ID 0x0002

static JSObject *starProto = NULL ;

class starPoll_t : public pollTimer_t {
public:
   starPoll_t( void );
   virtual ~starPoll_t( void );
   
   virtual void fire( void );
   void print( JSContext  *cx,
               void const *data,
               unsigned    len );

   void setHandler( jsval method, JSObject *obj );
#define STATUS_BUF_SIZE 512
   char                   prevData_[STATUS_BUF_SIZE];
   unsigned               prevLen_ ;
   struct usb_dev_handle *udev_ ;
   jsval                  statusHandler_ ;
   JSObject              *statusObj_ ;
   char                  asb_valid;
   char                  lastENQ;
   char                  lastEOT;
   char                  asbInitialized;
   char                  statusWaitTicks;
   char                  eotNext;
};

static int const inep  = 130; // find_ep(dev, config, interface, altsetting, USB_ENDPOINT_IN, USB_ENDPOINT_TYPE_BULK);
static int const outep = 1 ; // find_ep(dev, config, interface, altsetting, USB_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK);
extern int usb_debug ;

starPoll_t :: starPoll_t( void )
   : pollTimer_t()
   , udev_( 0 )
   , prevLen_( 0 )
   , statusHandler_( JSVAL_VOID )
   , statusObj_( 0 ), asb_valid(0), lastENQ(0), lastEOT(0), asbInitialized(0), statusWaitTicks(2), eotNext(0)
{
//   usb_debug = 2 ;
   JS_AddRoot( execContext_, &statusHandler_ );
   JS_AddRoot( execContext_, &statusObj_ );

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
         printf( "printer interface claimed\r\n" );
      } else {
         printf( "!!!usb_open of printer interface failed\r\n" );
      }
   } else {
      printf( "!!!printer interface not found\r\n" );
   }
}

void starPoll_t :: print( JSContext *cx, void const *data, unsigned len )
{
   unsigned    outBytes = len ;
   char const *nextOut = (char const *)data ;

   while( ( 0 < outBytes ) && ( 0 != udev_ ) )
   {
      unsigned toWrite = (4096 < outBytes) ? 4096 : outBytes ;

      int const numWritten = usb_bulk_write( udev_, outep,
                                             (char *)nextOut, toWrite, 1000 );
      statusWaitTicks = 2;
      lastENQ &= ~0x20;		//clear idle bit
      if( toWrite == numWritten )
      {
         outBytes -= numWritten ;
         nextOut  += numWritten ;
      }
      else if( 0 > numWritten )
      {
         JS_ReportError( cx, "Error writing to printer" );
         usb_close( udev_ );
         udev_ = 0 ;
      }
      else
      {
         JS_ReportError( cx, "printer stall: %u of %u\n", numWritten, toWrite );
         if( 0 <= usb_clear_halt( udev_, outep ) )
         {
            outBytes -= numWritten ;
            nextOut  += numWritten ;
         }
         else
         {
            JS_ReportError( cx, "Error resetting printer" );
            usb_close( udev_ );
            udev_ = 0 ;
         }
      }
   }
}

void starPoll_t :: setHandler( jsval method, JSObject *obj )
{
   statusHandler_ = method ;
   statusObj_     = obj ;
}

starPoll_t :: ~starPoll_t( void )
{
   if( udev_ )
   {
      usb_close( udev_ );
      udev_ = 0 ;
   }
   JS_RemoveRoot( execContext_, &statusHandler_ );
   JS_RemoveRoot( execContext_, &statusObj_ );
}
   
void starPoll_t :: fire( void )
{
   short availableReadLength = 0;
   if (udev_==0) {
      printf( "!!!!!Printer has been closed\n" );
      fflush( stdout );
      return;
   } else if( 0 <= usb_control_msg(udev_, (char) 0xc0, (char) 3, (short) 256, (short) 0, (char *) &availableReadLength, (short) 2, 100 ))
   {
      if( 0 < availableReadLength )
      {
         char inBuf[STATUS_BUF_SIZE];
         int numRead = usb_bulk_read(udev_, inep, inBuf, sizeof(inBuf), 100 );
         if( 0 < numRead ) {
            char change=0;
            statusWaitTicks = 0;
            if (asb_valid) {
               if ( ( numRead != prevLen_ ) || ( 0 != memcmp( prevData_, inBuf, prevLen_ ) ) ) {
                  for( int i = 0 ; i < numRead ; i++ ) printf( "%02x ", (unsigned char)inBuf[i] );
                  printf( "\n" );
                  if (numRead >= 9) {
                     memcpy( prevData_, inBuf, numRead );
                     prevLen_ = numRead ;
                     change = 1;
                  } else {
                    printf("ASB status too short\n");
                  }
               }
            } else {
               char lENQ = lastENQ;
               char lEOT = lastEOT;
               char weird = 0;
               for (int i=0; i<numRead; i++) {
                  char c = inBuf[i];
                  if ((c & 0x13)==0) { lENQ = c; eotNext = 1;}
                  else if ((c & 0x91)==0x10) { lEOT = c; eotNext = 0;}
                  else weird = 1;
               }

               if ((lENQ != lastENQ)||(lEOT != lastEOT)||weird) {
                  if (lENQ != lastENQ) {
                     lastENQ = lENQ;
                     printf( "ENQ status: %02x\n",lastENQ );
                  }
                  if (lEOT != lastEOT) {
                     lastEOT = lEOT;
                     printf( "EOT status: %02x\n",lastEOT );
                  }
                  if (weird) printf( "weird status:");
                  for( int i = 0 ; i < numRead ; i++ ) printf( "%02x ", (unsigned char)inBuf[i] );
                  printf( "\n" );
                  change = 1;
               }
            }
            if (change) if( JSVAL_VOID != statusHandler_ )
            {
               JSObject *scope = statusObj_ ;
               executeCode( statusObj_, statusHandler_, "starUSB:onStatusChange", 0, 0 );
            }
         } else printf( "read error %d\n", numRead );
      } // data available
	  else {
         if (statusWaitTicks==0) {
            static const char strAsbInvalid[] = {0x1b,0x1e,0x61,0x30};
            static const char strAsbValid[] = {0x1b,0x1e,0x61,0x31};
            static const char strEnq[] = {0x05};
            static const char strEot[] = {0x04};
            const char * p = NULL;
            int len = 0;
            if (asbInitialized==0) {
               if (asb_valid) {p = strAsbValid; len = sizeof(strAsbValid); }
               else {p = strAsbInvalid; len = sizeof(strAsbInvalid);}
            } else if (asb_valid==0) {
               if (eotNext) {p = strEot; len = sizeof(strEot);}
               else {p = strEnq; len = sizeof(strEnq);}
            }
            if (p) {
               int const numWritten = usb_bulk_write( udev_, outep,(char*)strEnq,len, 1000 );
               if (numWritten==4) asbInitialized = 1;
               statusWaitTicks = 10;	//wait a second before next write
            }
         } else statusWaitTicks--;
      }
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

static JSClass jsStarStatusClass_ = {
   "starStatus",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};


static JSBool jsGetStatus( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   starPoll_t *const star = (starPoll_t *)JS_GetInstancePrivate( cx, obj, &jsStarUSBClass_, NULL );
   if( ( 0 != star ) && ( 0 != star->udev_ ) )
   {
      struct status {
          // parsed
          bool offline;
          bool presenter;
          bool coveropen;
          bool paperempty;
          bool papernearempty;
          bool draweropen;
          bool stackerfull;
          bool etbavailable;
          unsigned char etbcounter;
          bool paperInPresenter;
          bool idle;
      };

      status st ;
      char valid = 0;
      if (star->asb_valid==0) {
         char lastENQ = star->lastENQ;
         char lastEOT = star->lastEOT;
         st.offline        = 0;
         st.presenter      = 0;
         st.coveropen      = (lastENQ & 0x04)!=0;
         st.paperempty     = ( ((star->eotNext) ?lastENQ : lastEOT) & 0x08)!=0;
         st.papernearempty = (lastEOT & 0x04)!=0;
         st.draweropen     = 0;
         st.stackerfull    = 0;
         st.etbavailable = 0;
         st.etbcounter = 0;
         st.paperInPresenter = 0;
         st.idle = lastENQ & 0x20;		//reception Buffer Empty
         valid = 1;
      } else if( 9 <= star->prevLen_ ) {
         st.offline        = (star->prevData_[2] & 0x08)!=0;
         st.presenter      = 1;
         st.coveropen      = (star->prevData_[2] & 0x20)!=0;
         st.paperempty     = (star->prevData_[5] & 0x08)!=0;
         st.papernearempty = (star->prevData_[5] & 0x04)!=0;
         st.draweropen     = (star->prevData_[2] & 0x04)!=0;
         st.stackerfull    = (star->prevData_[6] & 0x02)!=0;

         st.etbavailable = 1;
#define ASB7_ETB_COUNTER	0x6e
         int i = (star->prevData_[7] & ASB7_ETB_COUNTER);
         st.etbcounter = (i + (i&0x0e)) >> 2;
         st.paperInPresenter = (0 != (star->prevData_[8] & 0x0e));
         st.idle = 0;
         valid = 1;
      }
      if (valid) {
         JSObject *statObj = JS_NewObject( cx, &jsStarStatusClass_, 0, obj );
         *rval = OBJECT_TO_JSVAL(statObj); // root

#define MY_JS_DefineBoolProperty(cx,obj,name,val) JS_DefineProperty( cx, obj, name, \
   val ? JSVAL_TRUE : JSVAL_FALSE, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY )

#define MY_JS_DefineProperty(cx,obj,name,val) JS_DefineProperty( cx, obj, name, \
   val, 0, 0, JSPROP_ENUMERATE|JSPROP_PERMANENT|JSPROP_READONLY )

         MY_JS_DefineBoolProperty( cx, statObj, "presenter", st.presenter);
         MY_JS_DefineBoolProperty( cx, statObj, "paperInPresenter", st.paperInPresenter);
         MY_JS_DefineBoolProperty( cx, statObj, "paperNearEmpty", st.papernearempty);
         MY_JS_DefineBoolProperty( cx, statObj, "paperOut", st.paperempty);
         MY_JS_DefineProperty( cx, statObj, "counter", INT_TO_JSVAL(st.etbcounter));
         int errors = 0 ;
         MY_JS_DefineProperty( cx, statObj, "errors", INT_TO_JSVAL(errors));
         MY_JS_DefineBoolProperty( cx, statObj, "idle", st.idle);
      } else
         JS_ReportError( cx, "No printer status available" );
   } else
      JS_ReportError( cx, "printer closed!" );
   return JS_TRUE ;
}

static JSBool jsOnStatusChange( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   JSObject *handlerObj = obj ;
   if( ( ( 1 == argc ) || ( 2 == argc ) )
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( cx, argv[0] ) ) 
       &&
       ( ( 1 == argc )
         ||
         ( ( JSTYPE_OBJECT == JS_TypeOfValue( cx, argv[1] ) )
           &&
           ( 0 != ( handlerObj = JSVAL_TO_OBJECT( argv[1] ) ) ) ) ) )
   {
      starPoll_t *const star = (starPoll_t *)JS_GetInstancePrivate( cx, obj, &jsStarUSBClass_, NULL );
      if( ( 0 != star ) && ( 0 != star->udev_ ) )
      {
         star->setHandler( argv[0], handlerObj );
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "printer closed!" );
   }
   else
      JS_ReportError( cx, "Usage: starUSB.onStatusChange( method [, object] );" );
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
       ( 0 != ( rhObj = JSVAL_TO_OBJECT( argv[0] ) ) )
       &&
       JS_InstanceOf( cx, rhObj, &jsImageClass_, NULL ) )
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
            //
            // each row of output is either:
            //    skipLine command
            // or
            //    'b' m n bits...        where n * 256 + m is the number of bytes worth of bits
            //
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

JSClass jsStarUSBClass_ = {
  "starUSB",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsStarUSBFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec properties_[] = {
  {0,0,0}
};

static void printBitmap( starPoll_t &dev,
                         JSContext  *cx, 
                         JSObject   *bmpObj )
{
   jsval     vPixMap ;
   jsval     vWidth ;
   jsval     vHeight ;
   JSString *sPixMap ;

   if( JS_GetProperty( cx, bmpObj, "pixBuf", &vPixMap )
       &&
       JSVAL_IS_STRING( vPixMap )
       &&
       ( 0 != ( sPixMap = JSVAL_TO_STRING( vPixMap ) ) )
       &&
       JS_GetProperty( cx, bmpObj, "width", &vWidth )
       &&
       JSVAL_IS_INT( vWidth )
       &&
       JS_GetProperty( cx, bmpObj, "height", &vHeight )
       &&
       JSVAL_IS_INT( vHeight ) )
   {
      unsigned const bmWidth    = JSVAL_TO_INT( vWidth );
      unsigned       bmHeight   = JSVAL_TO_INT( vHeight );

      char outBuf[4096];
      char *nextOut = outBuf ;
      memcpy( nextOut, initPrinter, sizeof( initPrinter )-1 );
      nextOut += sizeof( initPrinter )-1 ;

      memcpy( nextOut, initRaster, sizeof( initRaster )-1 );
      nextOut += sizeof( initRaster )-1 ;
      
      int hSpecLen = sprintf( nextOut, "%s%d", setPageHeight, bmHeight ) + 1 ;
      nextOut += hSpecLen ;
      
      memcpy( nextOut, letterQuality, sizeof( letterQuality )-1 );
      nextOut += sizeof( letterQuality )-1 ;

// printf( "done sending header to printer: %u x %u\n", bmWidth, bmHeight );   

      unsigned const bytesIn = (bmWidth+7)/8 ;
      unsigned const inBytesPerRow = ((bmWidth+31)/32)*4 ;
      unsigned const maxBytesPerRow = 3 + bytesIn ;
      
// printf( "BYTES %u x %u, %u\n", bytesIn, inBytesPerRow, maxBytesPerRow );   
      //
      // flush buffer to device when (if) we get to this point
      //
      char *const lastStartOfLine = outBuf+sizeof(outBuf)-maxBytesPerRow ;
      JSString *const sPixMap = JSVAL_TO_STRING( vPixMap );
      char const *nextIn = JS_GetStringBytes( sPixMap );

      //
      // print data goes here.
      // each output line is either:
      //    skipLine
      // or 
      //    'b' m n bits...        where n * 256 + m is the number of bytes worth of bits
      //
      unsigned char const n = bytesIn/256 ;
      unsigned char const m = bytesIn&255 ;

      unsigned totalOut = 0 ;
/*
      hexDumper_t dumpIn( nextIn, inBytesPerRow );
      while( dumpIn.nextLine() )
         printf( "%s\n", dumpIn.getLine() );
*/

      unsigned const inPad = inBytesPerRow-bytesIn ;
      char *prevSkip = 0 ;

      for( unsigned y = 0 ; y < bmHeight ; y++ )
      {
         char *const startOfLine = nextOut ;
         *nextOut++ = 'b' ;
         *nextOut++ = (char)m ;
         *nextOut++ = (char)n ;

         // copy next line from input
         char *lastSet = startOfLine ;
         for( unsigned byte = 0 ; byte < bytesIn ; byte++ )
         {
            char const in = *nextIn++ ;
            if( in )
               lastSet = nextOut ;
            *nextOut++ = in ;
         }

         nextIn += inPad ;
         if( lastSet == startOfLine )
         {
            if( 0 == prevSkip )
               prevSkip = startOfLine ;

            nextOut = startOfLine ;
            memcpy( nextOut, skipLine, sizeof( skipLine )-1 );
            nextOut += sizeof( skipLine )-1 ;
         }
         else if( ++lastSet < nextOut )
         {
            if( prevSkip )
            {
               unsigned skipBytes  = (startOfLine - prevSkip);
               unsigned numSkipped = skipBytes/(sizeof(skipLine)-1) ;
               prevSkip = 0 ;
            } // could optimize this
            unsigned const lineBytes = lastSet - startOfLine - 3 ;
            startOfLine[1] = (char)(lineBytes & 255);
            startOfLine[2] = (char)(lineBytes / 256);
            nextOut = lastSet + 1 ;
         }
//         memset( nextOut, '\x0', bytesIn );
//         memset( nextOut, '\xff', bytesIn );
/*
         memcpy( nextOut, nextIn, bytesIn );
         nextOut += bytesIn ;
         nextIn  += inBytesPerRow ;
*/
         if( nextOut >= lastStartOfLine )
         {
// printf( "print: %p..%p\n", outBuf, nextOut );            
            dev.print( cx, outBuf, nextOut-outBuf );
            totalOut += nextOut-outBuf ;
            nextOut = outBuf ;
         }
      }

// printf( "done sending data to printer\n" );   

      memcpy( nextOut, formFeed, sizeof( formFeed )-1 );
      nextOut += sizeof( formFeed )-1 ;

      memcpy( nextOut, exitRaster, sizeof( exitRaster )-1 );
      nextOut += sizeof( exitRaster )-1 ;

      dev.print( cx, outBuf, nextOut-outBuf );
      totalOut += nextOut-outBuf ;

      assert( nextOut <= outBuf+sizeof(outBuf) );

printf( "%lu total bytes sent to printer\n", totalOut );
// printf( "done sending to printer\n" );   
   }
   else
      JS_ReportError( cx, "Invalid bitmap\n" );
}

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
         if( JS_InstanceOf( cx, rhObj, &jsAlphaMapClass_, NULL ) )
         {
            JS_ReportError( cx, "Can't (yet) convert from alphamap" );
         }
         else if( JS_InstanceOf( cx, rhObj, &jsImageClass_, NULL ) )
         {
            if( JS_CallFunctionName( cx, obj, "imageToString", argc, argv, rval )
                &&
                JSVAL_IS_STRING( *rval )
                &&
                ( 0 != ( sArg = JSVAL_TO_STRING( *rval ) ) ) )
            {
               star->print( cx, JS_GetStringBytes( sArg ), JS_GetStringLength( sArg ) );
            }
            else
               JS_ReportError( cx, "converting image to string\n" );
         }
         else if( JS_InstanceOf( cx, rhObj, &jsBitmapClass_, NULL ) )
         {
            printBitmap( *star, cx, rhObj );
         }
         else
            JS_ReportError( cx, "Unknown conversion" );
      }
      else if( ( 1 == argc )
               &&
               JSVAL_IS_STRING( argv[0] )
               &&
               ( 0 != ( sArg = JSVAL_TO_STRING( argv[0] ) ) ) )
      {
         star->print( cx, JS_GetStringBytes( sArg ), JS_GetStringLength( sArg ) );
      }
      else
      {
         JS_ReportError( cx, "Usage: printer.print( alphaMap|string )" );
         sArg = 0 ; // just in case
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
// constructor for the starUSB object
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

