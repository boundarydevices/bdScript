/*
 * Module jsBarcode.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: jsBarcode.cpp,v $
 * Revision 1.8  2003-05-21 23:24:32  tkisky
 * -added character delay to scanner
 *
 * Revision 1.7  2003/02/08 16:09:26  ericn
 * -added baud,parity,bits support
 *
 * Revision 1.6  2003/01/12 03:04:12  ericn
 * -added sendToScanner() and fakeBarcode()
 *
 * Revision 1.5  2003/01/05 01:58:15  ericn
 * -added identification of threads
 *
 * Revision 1.4  2002/12/18 04:12:25  ericn
 * -removed debug msg, added root for handler scope
 *
 * Revision 1.3  2002/12/01 00:02:50  ericn
 * -rooted barcode handler code
 *
 * Revision 1.2  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.1  2002/11/17 00:51:34  ericn
 * -Added Javascript barcode support
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsBarcode.h"
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include "codeQueue.h"
#include "jsGlobals.h"
#include "semClasses.h"

static std::string sBarcode_ ;
static std::string sSymbology_( "unknown" );
static jsval       sHandler_ = JSVAL_VOID ;
static JSObject   *handlerScope_ ;
static mutex_t     bcMutex_ ;
static JSBool
jsOnBarcode( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( ( 1 == argc )
       &&
       JSVAL_IS_STRING( argv[0] ) )
   {
      sHandler_ = argv[0];
      *rval = JSVAL_TRUE ;
   }

   return JS_TRUE ;
}

static JSBool
jsGetBarcode( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   JSString *jsBC = JS_NewStringCopyN( cx, sBarcode_.c_str(), sBarcode_.size() );
   if( jsBC )
   {
      *rval = STRING_TO_JSVAL( jsBC );
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static JSBool
jsGetBarcodeSymbology( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   JSString *jsBC = JS_NewStringCopyN( cx, sSymbology_.c_str(), sSymbology_.size() );
   if( jsBC )
   {
      *rval = STRING_TO_JSVAL( jsBC );
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static int       bcDev_ ;
static int scannerDelay=0;

static JSBool
jsSendToScanner( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( 1 == argc )
   {
      JSString *jsMsg = JSVAL_TO_STRING( argv[0] );
      char * p = JS_GetStringBytes( jsMsg );
      int len = JS_GetStringLength( jsMsg );
      int numWritten;
      if (scannerDelay==0)
         numWritten = write( bcDev_, p, len );
      else {
         numWritten=0;
	 while (len) {
	    usleep(scannerDelay);
            numWritten += write( bcDev_, p, 1 );
	    p++;
	    len--;
         }
      }
      *rval = INT_TO_JSVAL( numWritten );
   }
   else
   {
      *rval = JSVAL_FALSE ;
      JS_ReportError( cx, "Usage: sendToScanner( \'string\' );" );
   }

   return JS_TRUE ;
}

static JSBool
jsFakeBarcode( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( 1 == argc )
   {
      JSString *jsMsg = JSVAL_TO_STRING( argv[0] );
      mutexLock_t lock( bcMutex_ );
      sBarcode_.assign( JS_GetStringBytes( jsMsg ), JS_GetStringLength( jsMsg ) );

      if( ( JSVAL_VOID != sHandler_ ) && ( 0 != handlerScope_ ) )
         queueSource( handlerScope_, sHandler_, "onBarcode" );
      
      *rval = JSVAL_TRUE ;
   }
   else
   {
      *rval = JSVAL_FALSE ;
      JS_ReportError( cx, "Usage: fakeBarcode( \'string\' );" );
   }

   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"onBarcode",            jsOnBarcode,             1 },
    {"getBarcode",           jsGetBarcode,            0 },
    {"getBarcodeSymbology",  jsGetBarcodeSymbology,   0 },
    {"sendToScanner",        jsSendToScanner,         1 },
    {"fakeBarcode",          jsFakeBarcode,           1 },
    {0}
};

static JSPropertySpec scannerProperties_[] = {
  {0,0,0}
};

static char const bcDeviceName_[] = {
   "/dev/ttyS2"
};

static void *barcodeThread( void *arg )
{
printf( "barcodeReader %p (id %x)\n", &arg, pthread_self() );   
   bcDev_ = open( bcDeviceName_, O_RDWR | O_NOCTTY );
   if( 0 <= bcDev_ )
   {
      struct termios oldTermState;
      tcgetattr(bcDev_,&oldTermState);

      /* set raw mode for keyboard input */
      struct termios newTermState = oldTermState;
      newTermState.c_cc[VMIN] = 1;
      newTermState.c_cc[VTIME] = 0;
      
//      cfsetispeed(&newTermState, B9600 );
//      cfsetospeed(&newTermState, B9600 );

//      newTermState.c_cflag &= ~(PARENB|CSTOPB|CSIZE|CRTSCTS);	// Mask character size to 8 bits, no parity, Disable hardware flow control
//      newTermState.c_cflag |= (CLOCAL | CREAD|CS8);		// Select 8 data bits
      newTermState.c_cflag &= ~(CSTOPB|CRTSCTS);	// Mask character size to 8 bits, no parity, Disable hardware flow control
      newTermState.c_cflag |= (CLOCAL | CREAD);		// Select 8 data bits
      newTermState.c_lflag &= ~(ICANON | ECHO | ISIG);	// set raw mode for input
      newTermState.c_iflag &= ~(IXON | IXOFF | IXANY|INLCR|ICRNL|IUCLC);	//no software flow control
      newTermState.c_oflag &= ~OPOST;			//raw output
      
      int result = tcsetattr( bcDev_, TCSANOW, &newTermState );
      if( 0 == result )
      {
         char inBuf[256];
         int  numRead ;
         while( 0 <= ( numRead = read( bcDev_, inBuf, sizeof( inBuf ) - 1 ) ) )
         {
            if( 0 < numRead )
            {
               inBuf[numRead] = '\0' ;
               mutexLock_t lock( bcMutex_ );
               sBarcode_ = inBuf ;
               
               if( ( JSVAL_VOID != sHandler_ ) && ( 0 != handlerScope_ ) )
               {
                  queueSource( handlerScope_, sHandler_, "onBarcode" );
               }
               else
                  printf( "no barcode handler\n" );
            }
            else
            {
               fprintf( stderr, "null read\n" );
               break;
            }
         }
      }
      else
         fprintf( stderr, "tcsetattr %m\n" );

      close( bcDev_ );
   }
   else
      fprintf( stderr, "Error %m opening device %s\n", bcDeviceName_ );
   
   return 0 ;
}

JSClass jsScannerClass_ = {
  "scanner",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

//
// constructor for the scanner object
//
static JSBool jsScanner( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   obj = JS_NewObject( cx, &jsScannerClass_, NULL, NULL );

   if( obj )
   {
      *rval = OBJECT_TO_JSVAL(obj);
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

static unsigned const _standardBauds[] = {
   0,
   50,
   75,
   110,
   134,
   150,
   200,
   300,
   600,
   1200,
   1800,
   2400,
   4800,
   9600,
   19200,
   38400
};
static unsigned const numStandardBauds = sizeof( _standardBauds )/sizeof( _standardBauds[0] );

static unsigned const _highSpeedMask = 0010000 ;
static unsigned const _highSpeedBauds[] = {
   0,
   57600,  
   115200, 
   230400, 
   460800, 
   500000, 
   576000, 
   921600, 
   1000000,
   1152000,
   1500000,
   2000000,
   2500000,
   3000000,
   3500000,
   4000000 
};

static unsigned const numHighSpeedBauds = sizeof( _highSpeedBauds )/sizeof( _highSpeedBauds[0] );

static JSBool
jsGetScannerBaud( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   struct termios oldTermState;
   tcgetattr(bcDev_,&oldTermState);
   unsigned short baudIdx = cfgetispeed(&oldTermState);
   unsigned baud ;
   if( baudIdx < numStandardBauds )
   {
      baud = _standardBauds[baudIdx];
   }
   else if( 0 != ( baudIdx & _highSpeedMask ) )
   {
      baudIdx &= ~_highSpeedMask ;
      if( baudIdx < numHighSpeedBauds )
      {
         baud = _highSpeedBauds[baudIdx];
      }
      else
         baud = 0xbeefdead ;
   }
   else
      baud = 0xdeadbeef ;

   *rval = INT_TO_JSVAL( baud );
   return JS_TRUE ;
}

static JSBool
jsSetScannerBaud( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
   {
      unsigned baudIdx ;
      bool haveBaud = false ;
      
      int const baud = JSVAL_TO_INT( argv[0] );
      unsigned i ;
      for( i = 0 ; i < numStandardBauds ; i++ )
      {
         if( _standardBauds[i] == baud )
         {
            haveBaud = true ;
            baudIdx = i ;
            break;
         }
      }
      
      if( !haveBaud )
      {
         for( i = 0 ; i < numHighSpeedBauds ; i++ )
         {
            if( _highSpeedBauds[i] == baud )
            {
               haveBaud = true ;
               baudIdx = i | _highSpeedMask ;
               break;
            }
         }
      }

      if( haveBaud )
      {
         struct termios termState;
         tcgetattr(bcDev_,&termState);
         cfsetispeed( &termState, baudIdx );
         cfsetospeed( &termState, baudIdx );
      
         int result = tcsetattr( bcDev_, TCSANOW, &termState );
         if( 0 == result )
         {
            *rval = JSVAL_TRUE ;
         }
         else
            perror( "setBaud" );
      }
      else
         JS_ReportError( cx, "unsupported baud rate" );
   }
   else
      JS_ReportError( cx, "Usage: scanner.setBaud( 9600 );" );

   return JS_TRUE ;
}

static JSBool
jsGetScannerParity( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   struct termios oldTermState;
   tcgetattr(bcDev_,&oldTermState);
   char const *parity ;
   if( oldTermState.c_cflag & PARENB )
   {
      if( oldTermState.c_cflag & PARODD )
         parity = "O" ;
      else
         parity = "E" ;
   }
   else
      parity = "N" ;
   
   *rval = STRING_TO_JSVAL( JS_NewStringCopyN( cx, parity, 1 ) );
   
   return JS_TRUE ;
}

static JSBool
jsSetScannerParity( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      struct termios termState;
      tcgetattr(bcDev_,&termState);
      
      JSString *sArg = JSVAL_TO_STRING( argv[0] );
      char const cParity = toupper( *JS_GetStringBytes( sArg ) );
      bool foundParity = false ;
      if( 'E' == cParity )
      {
         termState.c_cflag |= PARENB ;
         termState.c_cflag &= ~PARODD ;
         foundParity = true ;
      }
      else if( 'N' == cParity )
      {
         termState.c_cflag &= ~PARENB ;
         foundParity = true ;
      }
      else if( 'O' == cParity )
      {
         termState.c_cflag |= PARENB | PARODD ;
         foundParity = true ;
      }
      else
         JS_ReportError( cx, "invalid parity <0x%02x>\n", cParity );

      if( foundParity )
      {
         int result = tcsetattr( bcDev_, TCSANOW, &termState );
         if( 0 == result )
         {
            *rval = JSVAL_TRUE ;
         }
         else
            perror( "setParity" );
      }
   }
   else
      JS_ReportError( cx, "Usage: scanner.setParity( 'E'|'O'|'N' );" );

   return JS_TRUE ;
}

static JSBool
jsGetScannerBits( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   struct termios oldTermState;
   tcgetattr(bcDev_,&oldTermState);
   unsigned csMask = oldTermState.c_cflag & CSIZE ;
   unsigned charLen = 0 ;
   if( CS8 == csMask )
      charLen = 8 ;
   else if( CS7 == csMask )
      charLen = 7 ;
   else if( CS6 == csMask )
      charLen = 6 ;
   else if( CS5 == csMask )
      charLen = 5 ;
   *rval = INT_TO_JSVAL( charLen );
   return JS_TRUE ;
}

static unsigned csLengthMasks_[] = {
   CS5,
   CS6,
   CS7,
   CS8
};

static JSBool
jsSetScannerBits( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
   {
      int const bits = JSVAL_TO_INT( argv[0] );
      if( ( 5 <= bits ) && ( 8 >= bits ) )
      {
         struct termios termState;
         tcgetattr(bcDev_,&termState);
         
         termState.c_cflag &= ~CSIZE ;
         termState.c_cflag |= csLengthMasks_[bits-5];
      
         int result = tcsetattr( bcDev_, TCSANOW, &termState );
         if( 0 == result )
         {
            *rval = JSVAL_TRUE ;
         }
         else
            perror( "setBits" );
      }
      else
         JS_ReportError( cx, "unsupported character length" );
   }
   else
      JS_ReportError( cx, "Usage: scanner.setBits( 7|8 );" );
   
   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}


static JSBool
jsGetScannerDelay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = INT_TO_JSVAL( scannerDelay );
   return JS_TRUE ;
}

static JSBool
jsSetScannerDelay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
   {
      int const delay = JSVAL_TO_INT( argv[0] );
      if( ( 0 <= delay ) && ( delay <= 500000 ) )
      {
         scannerDelay= delay;
      }
      else
         JS_ReportError( cx, "must be 0-500000" );
   }
   else
      JS_ReportError( cx, "Usage: scanner.setBits( 0-500000);" );
   
   *rval = JSVAL_FALSE ;
   return JS_TRUE ;
}

static JSFunctionSpec scanner_methods[] = {
   { "getBaud",      jsGetScannerBaud,   0,0,0 },
   { "setBaud",      jsSetScannerBaud,   0,0,0 },
   { "getParity",    jsGetScannerParity, 0,0,0 },
   { "setParity",    jsSetScannerParity, 0,0,0 },
   { "getBits",      jsGetScannerBits,   0,0,0 },
   { "setBits",      jsSetScannerBits,   0,0,0 },
   { "getDelay",     jsGetScannerDelay,   0,0,0 },
   { "setDelay",     jsSetScannerDelay,   0,0,0 },
   { 0 }
};

static pthread_t threadHandle_ = 0 ;

bool initJSBarcode( JSContext *cx, JSObject *glob )
{
   if( JS_DefineFunctions( cx, glob, _functions) )
   {
      JSObject *rval = JS_InitClass( cx, glob, NULL, &jsScannerClass_,
                                     jsScanner, 1,
                                     scannerProperties_, 
                                     scanner_methods,
                                     0, 0 );
      if( rval )
      {
         JSObject *obj = JS_NewObject( cx, &jsScannerClass_, NULL, NULL );
         if( obj )
         {
            JS_DefineProperty( cx, glob, "scanner", 
                               OBJECT_TO_JSVAL( obj ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
         }
         else
            JS_ReportError( cx, "initializing scanner global" );
      }
      else
         JS_ReportError( cx, "initializing scanner class" );

      int const create = pthread_create( &threadHandle_, 0, barcodeThread, 0 );
      if( 0 == create )
      {
         JS_AddRoot( cx, &sHandler_ );
         JS_AddRoot( cx, &handlerScope_ );
         handlerScope_ = glob ;
         return true ;
      }
      else
         threadHandle_ = 0 ;
   }

   return false ;
}

void stopBarcodeThread()
{
   if( 0 <= bcDev_ )
      close( bcDev_ );

   if( threadHandle_ )
   {
      pthread_cancel( threadHandle_ );
      void *exitStat ;
      pthread_join( threadHandle_, &exitStat );
   }
   
   JS_RemoveRoot( execContext_, &sHandler_ );
}


