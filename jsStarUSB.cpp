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
 * Revision 1.12  2006-09-22 01:45:18  ericn
 * -retry claim, release on close
 *
 * Revision 1.11  2006/08/16 02:35:09  ericn
 * -use *rE1 for no cut command
 *
 * Revision 1.10  2006/08/07 18:21:26  tkisky
 * -change partialCut to cutMode
 *
 * Revision 1.9  2006/08/07 16:49:09  tkisky
 * -partial cut flag
 *
 * Revision 1.8  2006/05/14 14:39:55  ericn
 * -new deviceId, debugPrint
 *
 * Revision 1.7  2005/11/06 00:49:41  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.6  2005/03/06 07:37:49  tkisky
 * -work around for older star printer software
 *
 * Revision 1.5  2004/12/05 00:56:11  tkisky
 * -minor
 *
 * Revision 1.4  2004/11/16 07:37:22  tkisky
 * -fix letter quality,add reverse for top margin
 *
 * Revision 1.3  2004/09/10 19:20:15  tkisky
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

//#define DEBUGPRINT
#include "debugPrint.h"

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
   , prevLen_( 0 )
   , udev_( 0 )
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
   struct usb_device *dev = 0 ;

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
         int iterations = 0 ;
         do { 
            int result = usb_claim_interface( udev_, intNum );
            if( 0 != result ){
               debugPrint( "Error claiming usb interface!\n" );
               sleep( 1 );
            }
            else {
               debugPrint( "claimed usb interface\n" );
               break ;
            }
         } while( ++iterations < 10 );

         set( 100 );
         debugPrint( "printer interface %d claimed\r\n", intNum );
      } else {
         debugPrint( "!!!usb_open of printer interface failed\r\n" );
      }
   } else {
      debugPrint( "!!!printer interface not found\r\n" );
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
      if( toWrite == (unsigned)numWritten )
      {
         outBytes -= numWritten ;
         nextOut  += numWritten ;
      }
      else if( 0 > numWritten )
      {
         JS_ReportError( cx, "Error writing to printer" );
         usb_release_interface( udev_, 0 );
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
            usb_release_interface( udev_, 0 );
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
      if( 0 == usb_release_interface( udev_, 0 ) )
      {
         debugPrint( "released printer interface\n" );
      }
      else
         debugPrint( "Error releasing printer interface\n" );
      debugPrint( "device closed\n" );
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
      debugPrint( "!!!!!Printer has been closed\n" );
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
            if (asb_valid || ((numRead >= 9) && ((inBuf[0] & 0x13) && ((inBuf[0] & 0x91)!=10))) ) {
               if ( ( (unsigned)numRead != prevLen_ ) || ( 0 != memcmp( prevData_, inBuf, prevLen_ ) ) ) {
#ifdef DEBUGPRINT               
                  for( int i = 0 ; i < numRead ; i++ ) 
                     debugPrint( "%02x ", (unsigned char)inBuf[i] );
                  debugPrint( "\n" );
#endif                  
                  if (numRead >= 9) {
                     memcpy( prevData_, inBuf, numRead );
                     prevLen_ = numRead ;
                     change = 1;
                  } else {
                    debugPrint("ASB status too short\n");
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
                     debugPrint( "ENQ status: %02x\n",lastENQ );
                  }
                  if (lEOT != lastEOT) {
                     lastEOT = lEOT;
                     debugPrint( "EOT status: %02x\n",lastEOT );
                  }
                  if (weird) 
                     debugPrint( "weird status:");
#ifdef DEBUGPRINT               
                  for( int i = 0 ; i < numRead ; i++ ) 
                     debugPrint( "%02x ", (unsigned char)inBuf[i] );
                  debugPrint( "\n" );
#endif                  
                  change = 1;
               }
            }
            if (change) if( JSVAL_VOID != statusHandler_ )
            {
               executeCode( statusObj_, statusHandler_, "starUSB:onStatusChange", 0, 0 );
            }
         } else debugPrint( "read error %d\n", numRead );
      } // data available
	  else {
         if (statusWaitTicks==0) {
            static const char strAsbInvalid[] = {0x1b,0x1e,0x61,0x0};
            static const char strAsbValid[] = {0x1b,0x1e,0x61,0x1};
            static const char strEotEnq[] = {0x04, 0x05};
            static const char strAsbStatus[] = {0x1b,0x06,0x01};	//esc ack soh
            const char * p = NULL;
            int len = 0;
            if (asbInitialized==0) {
               if (asb_valid) {p = strAsbValid; len = sizeof(strAsbValid); }
               else {p = strAsbInvalid; len = sizeof(strAsbInvalid); }
            } else {
               if (asb_valid==0) { p = strEotEnq; len = sizeof(strEotEnq); }
               else { p = strAsbStatus; len = sizeof(strAsbStatus); }
            }
            if (p) {
               int const numWritten = usb_bulk_write( udev_, outep,(char*)p,len, 1000 );
               if (numWritten==4) asbInitialized = 1;
               statusWaitTicks = (numWritten==len) ? 40 : 10;	//wait up to 4 seconds for response, before next write
            }
         } else statusWaitTicks--;
      }
   }
   else
      debugPrint( "Error issuing ctrl msg\n" );

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
   starPoll_t *const star = (starPoll_t *)JS_GetInstancePrivate( cx, obj, &jsStarUSBClass_, NULL );
   if( ( 0 != star ) && ( 0 != star->udev_ ) ){
      JS_SetPrivate( cx, obj, 0 );
      delete star ;
   }

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
          bool paperOut;
          bool paperNearEmpty;
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
         st.paperOut       = ( ((star->eotNext) ?lastENQ : lastEOT) & 0x08)!=0;
         st.paperNearEmpty = (lastEOT & 0x04)!=0;
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
         st.paperOut       = (star->prevData_[5] & 0x08)!=0;
         st.paperNearEmpty = (star->prevData_[5] & 0x04)!=0;
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
         MY_JS_DefineBoolProperty( cx, statObj, "paperNearEmpty", st.paperNearEmpty);
         MY_JS_DefineBoolProperty( cx, statObj, "paperOut", st.paperOut);
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

//include 0 terminator
static char const reverseForTopMargin[] = "\x1b*rT1";

//include 0 terminator
static char const draftQuality[] = {
   "\x1b*rQ0"       // 0 == high speed, 1 == normal, 2 == letter quality
};

//include 0 terminator
static char const normalQuality[] = {
   "\x1b*rQ1"       // 0 == high speed, 1 == normal, 2 == letter quality
};

//include 0 terminator
static char const letterQuality[] = {
   "\x1b*rQ2"       // 0 == high speed, 1 == normal, 2 == letter quality
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

//include 0 terminator
static char const skipLine[] = {
   "\x1b*rY1"
};

//include 0 terminator
static char const formFeed[] = { "\x1b\x0C"};

//include 0 terminator
static char const partialCut[] = {   "\x1b*rE13"};
//static char const stdPartialCut[] = {   "\x1b\x64\x03"};

static char const noCut[] = { "\x1b*rE1"};

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
                  memcpy( startOfLine, skipLine, sizeof( skipLine ) );
                  nextOut = startOfLine + sizeof( skipLine );
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

#if DEBUGPRINT
void DbgPrintHex(unsigned char* p, int cnt)
{
	int i = 0;
	while (cnt) {
		unsigned int ch = *p++;
		cnt--;
		printf("%02x ",ch);
		i++;
		if ( (i & 0xf)==0) printf("\r\n");
	}
	if (i & 0xf) printf("\r\n");
}
#else
#define DbgPrintHex(a,b)
#endif

#define PM_NO_CUT 0
#define PM_PARTIAL_CUT 1
#define PM_FULL_CUT 2

static void printBitmap( starPoll_t &dev, JSContext  *cx, JSObject   *bmpObj, int cutMode )
{
	jsval     vPixMap ;
	jsval     vWidth ;
	jsval     vHeight ;
	JSString *sPixMap ;

	if( JS_GetProperty( cx, bmpObj, "pixBuf", &vPixMap ) &&
		JSVAL_IS_STRING( vPixMap ) &&
		( 0 != ( sPixMap = JSVAL_TO_STRING( vPixMap ) ) ) &&
		JS_GetProperty( cx, bmpObj, "width", &vWidth ) &&
		JSVAL_IS_INT( vWidth ) &&
		JS_GetProperty( cx, bmpObj, "height", &vHeight ) &&
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

		memcpy( nextOut, reverseForTopMargin, sizeof( reverseForTopMargin ) );
		nextOut += sizeof( reverseForTopMargin ) ;      //include terminating null

		if (cutMode==PM_PARTIAL_CUT) {
			memcpy( nextOut, partialCut, sizeof( partialCut ) );    //include terminating null
			nextOut += sizeof( partialCut );
debugPrint("moved partial cut string\n");
		} else if (cutMode==PM_NO_CUT) {
			memcpy( nextOut, noCut, sizeof( noCut ) );	//include terminating null
			nextOut += sizeof( noCut );
		}
		//avoid a bug in printer software where 3 ft ticket is printed if bmHeight is small
		int hSpecLen = sprintf( nextOut, "%s%d", setPageHeight, (bmHeight >= 200)? bmHeight : 0 ) + 1 ;
		nextOut += hSpecLen ;

		memcpy( nextOut, letterQuality, sizeof( letterQuality ) ); //include 0 terminator
		nextOut += sizeof( letterQuality );


debugPrint( "done sending header to printer: %u x %u\n", bmWidth, bmHeight );

		unsigned const bytesIn = (bmWidth+7)/8 ;
		unsigned const inBytesPerRow = ((bmWidth+31)/32)*4 ;
		unsigned const maxBytesPerRow = 3 + bytesIn ;

debugPrint( "BYTES %u x %u, %u\n", bytesIn, inBytesPerRow, maxBytesPerRow );
		//
		// flush buffer to device when (if) we get to this point
		//
		char *const lastStartOfLine = outBuf+sizeof(outBuf)-maxBytesPerRow -20;	//cushion for skip lines formfeed and exit raster

		JSString *const sPixMap = JSVAL_TO_STRING( vPixMap );
		char const *nextIn = JS_GetStringBytes( sPixMap );
		char const *inEndOfData = nextIn + (inBytesPerRow* bmHeight);
		//
		// print data goes here.
		// each output line is either:
		//    skipLine
		// or
		//    'b' m n bits...        where n * 256 + m is the number of bytes worth of bits
		//

		unsigned totalOut = 0 ;
/*
		hexDumper_t dumpIn( nextIn, inBytesPerRow );
		while( dumpIn.nextLine() )
			printf( "%s\n", dumpIn.getLine() );
*/

		unsigned const inPad = inBytesPerRow-bytesIn ;
		while (nextIn < inEndOfData) {
			int skipCnt=0;
			const char * startIn = nextIn;
			const char * inEndOfline = nextIn + bytesIn;
			while (1) {
				if (*nextIn++) {
					nextIn--;
					break;
				} else if (nextIn >= inEndOfline) {
					nextIn += inPad ;
					startIn = nextIn;
					inEndOfline = nextIn + bytesIn;
					skipCnt++;
					if (nextIn >= inEndOfData) break;
				}
			}

			if (skipCnt) {
				memcpy( nextOut, skipLine, sizeof( skipLine )-2 );
				nextOut += sizeof( skipLine )-2 ;	//don't include the 1 or \x00
				hSpecLen = sprintf( nextOut, "%d", skipCnt ) + 1 ;
				nextOut += hSpecLen ;
			}
			if (nextIn >= inEndOfData) break;
			int zeroBytes = nextIn - startIn;

			const char* p = nextIn + (bytesIn - zeroBytes) -1;
			while (p >= nextIn) {
				if (*p) break;
				p--;
			}
			int copyCnt = (p+1)- nextIn;
			*nextOut++ = 'b' ;
			*nextOut++ = (char)(zeroBytes+copyCnt);
			*nextOut++ = (char)((zeroBytes+copyCnt)>>8) ;

			if (zeroBytes) { memset( nextOut, 0, zeroBytes); nextOut += zeroBytes; }
			memcpy( nextOut, nextIn, copyCnt);
			nextOut += copyCnt;
			nextIn += inBytesPerRow - zeroBytes;
			if( nextOut >= lastStartOfLine ) {
debugPrint( "print: %p..%p\n", outBuf, nextOut );
				dev.print( cx, outBuf, nextOut-outBuf );
				totalOut += nextOut-outBuf ;
				nextOut = outBuf ;
			}
		}

debugPrint( "done sending data to printer\n" );


//		memcpy( nextOut, partialCut, sizeof( partialCut ) );	//include terminating null
//		nextOut += sizeof( partialCut );

//		memcpy( nextOut, formFeed, sizeof( formFeed ) );	//include terminating null
//		nextOut += sizeof( formFeed );

//		memcpy( nextOut, exitRaster, sizeof( exitRaster )-1 );
//		nextOut += sizeof( exitRaster )-1 ;

//		memcpy( nextOut, stdPartialCut, sizeof( stdPartialCut) -1 );
//		nextOut += sizeof( stdPartialCut )-1;

		memcpy( nextOut, exitRaster, sizeof( exitRaster )-1 );
		nextOut += sizeof( exitRaster )-1 ;

		dev.print( cx, outBuf, nextOut-outBuf );
		totalOut += nextOut-outBuf ;

		DbgPrintHex((unsigned char *)outBuf,nextOut-outBuf);

		assert( nextOut <= outBuf+sizeof(outBuf) );

		debugPrint( "%lu total bytes sent to printer\n", totalOut );
		debugPrint( "done sending to printer\n" );
	} else JS_ReportError( cx, "Invalid bitmap\n" );
}

static JSBool jsPrint( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   JSObject *rhObj ;
   JSString *sArg = 0 ;
   starPoll_t *const star = (starPoll_t *)JS_GetInstancePrivate( cx, obj, &jsStarUSBClass_, NULL );
   if( ( 0 != star ) && ( 0 != star->udev_ ) )
   {
      if( ( 1 <= argc )
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
	    int cutMode = PM_FULL_CUT;
	    if ((2 == argc) && JSVAL_IS_INT(argv[1])) {
		cutMode = JSVAL_TO_INT(argv[1]);
	    }
debugPrint( "cutMode:%u argc:%u\n", cutMode,argc );
            printBitmap( *star, cx, rhObj, cutMode );
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
         JS_ReportError( cx, "Usage: printer.print( alphaMap|string, [cutMode=2] )" );
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
      s = JS_NewStringCopyN( cx, formFeed, sizeof( formFeed ));
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
      s = JS_NewStringCopyN( cx, draftQuality, sizeof( draftQuality ) );	//include 0 terminator
      JS_DefineProperty( cx, star, "draftQuality",
                         STRING_TO_JSVAL( s ),
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      s = JS_NewStringCopyN( cx, normalQuality, sizeof( normalQuality ) );	//include 0 terminator
      JS_DefineProperty( cx, star, "normalQuality",
                         STRING_TO_JSVAL( s ),
                         0, 0,
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      s = JS_NewStringCopyN( cx, letterQuality, sizeof( letterQuality ) );	//include 0 terminator
      JS_DefineProperty( cx, star, "letterQuality",
                         STRING_TO_JSVAL( s ),
                         0, 0,
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
                         
      s = JS_NewStringCopyZ( cx, "Star TUP900" );
      JS_DefineProperty( cx, star, "deviceId",
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

