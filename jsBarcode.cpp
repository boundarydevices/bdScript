/*
 * Module jsBarcode.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: jsBarcode.cpp,v $
 * Revision 1.6  2003-01-12 03:04:12  ericn
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

static JSBool
jsSendToScanner( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   if( 1 == argc )
   {
      JSString *jsMsg = JSVAL_TO_STRING( argv[0] );
      int const numWritten = write( bcDev_, JS_GetStringBytes( jsMsg ), JS_GetStringLength( jsMsg ) );
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
      
      cfsetispeed(&newTermState, B9600 );
      cfsetospeed(&newTermState, B9600 );

      newTermState.c_cflag &= ~(PARENB|CSTOPB|CSIZE|CRTSCTS);	// Mask character size to 8 bits, no parity, Disable hardware flow control
      newTermState.c_cflag |= (CLOCAL | CREAD|CS8);		// Select 8 data bits
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

static pthread_t threadHandle_ = 0 ;

bool initJSBarcode( JSContext *cx, JSObject *glob )
{
   if( JS_DefineFunctions( cx, glob, _functions) )
   {
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


