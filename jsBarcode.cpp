/*
 * Module jsBarcode.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: jsBarcode.cpp,v $
 * Revision 1.2  2002-11-30 18:52:57  ericn
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

static std::string sBarcode_ ;
static std::string sSymbology_( "unknown" );
static jsval       sHandler_ = JSVAL_VOID ;
static JSObject   *handlerScope_ ;

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

static JSFunctionSpec _functions[] = {
    {"onBarcode",            jsOnBarcode,             1 },
    {"getBarcode",           jsGetBarcode,            0 },
    {"getBarcodeSymbology",  jsGetBarcodeSymbology,   0 },
    {0}
};

static char const bcDeviceName_[] = {
   "/dev/ttyS2"
};

static int       bcDev_ ;

static void *barcodeThread( void *arg )
{
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
               sBarcode_ = inBuf ;
               printf( "%d:%s\n", numRead, inBuf );
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
}


