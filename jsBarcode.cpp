/*
 * Module jsBarcode.cpp
 *
 * This module defines the base methods of the
 * barcodeReader class as described in jsBarcode.h
 * and the initialization routine.
 *
 *
 * Change History : 
 *
 * $Log: jsBarcode.cpp,v $
 * Revision 1.9  2003-06-08 15:19:07  ericn
 * -objectified scanner interface
 *
 * Revision 1.8  2003/05/21 23:24:32  tkisky
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
#include <poll.h>
#include <errno.h>
#include "js/jscntxt.h"


struct bcrParams_t {
   jsval     object_ ;
   JSObject *scope_ ;
   int       fd_ ;
   char      terminator_ ;
   pthread_t threadHandle_ ;
   unsigned  outputDelay_ ;
};

static void deliverBarcodeData( bcrParams_t const &params,
                                std::string const &bcData )
{
   mutexLock_t lock( execMutex_ );
   jsval onbarcode ;
   if( JS_GetProperty( execContext_, params.scope_, "onData", &onbarcode ) )
   {
      JSString *jsBarcode = JS_NewStringCopyN( execContext_, bcData.c_str(), bcData.size() );
      if( jsBarcode )
      {
         jsval bcv[1] = { STRING_TO_JSVAL( jsBarcode ) };
         queueSource( params.scope_, onbarcode, "onData", 1, bcv );
      }
      else
         fprintf( stderr, "Error allocating barcode\n" );
   }
   else
      printf( "no barcode handler\n" );
}

static void *barcodeThread( void *arg )
{
   bcrParams_t const *params = (bcrParams_t *)arg ;
printf( "barcodeReader %p (id %x)\n", &arg, pthread_self() );   
   struct termios oldTermState;
   tcgetattr(params->fd_,&oldTermState);

   /* set raw mode for keyboard input */
   struct termios newTermState = oldTermState;
   newTermState.c_cc[VMIN] = 1;

   //
   // Note that this doesn't appear to work!
   // Reads always seem to be terminated at 16 chars!
   //
   newTermState.c_cc[VTIME] = 0; // 1/10th's of a second, see http://www.opengroup.org/onlinepubs/007908799/xbd/termios.html

   newTermState.c_cflag &= ~(CSTOPB|CRTSCTS);           // Mask character size to 8 bits, no parity, Disable hardware flow control
   newTermState.c_cflag |= (CLOCAL | CREAD);            // Select 8 data bits
   newTermState.c_lflag &= ~(ICANON | ECHO | ISIG);     // set raw mode for input
   newTermState.c_iflag &= ~(IXON | IXOFF | IXANY|INLCR|ICRNL|IUCLC);   //no software flow control
   newTermState.c_oflag &= ~OPOST;                      //raw output
   
   int result = tcsetattr( params->fd_, TCSANOW, &newTermState );
   if( 0 == result )
   {
      //
      // outer loop:
      //    wait forever (or until error) for data
      //       if terminator
      //          do
      //             read what's there
      //             if terminated
      //                deliver
      //             else
      //                wait for timeout
      //          while not terminated and end-of-input
      //       else
      //          do 
      //             read what's there
      //             wait up to 10ms for more
      //          while( !timedout )
      //
      while( 1 )
      {
         pollfd filedes ;
         filedes.fd = params->fd_ ;
         filedes.events = POLLIN ;
         
         int pollRes = poll( &filedes, 1, 0xFFFFFFFF );
         if( 0 < pollRes )
         {
            
            if( params->terminator_ )
            {
               std::string sBarcode ;
               while( 1 )
               {
                  char inBuf[256];
                  int numRead = read( params->fd_, inBuf, sizeof( inBuf ) - 1 );
                  if( 0 < numRead )
                  {
                     for( unsigned i = 0 ; i < numRead ; i++ )
                     {
                        sBarcode += inBuf[i];
                        if( params->terminator_ == inBuf[i] )
                        {
                           if( 0 < sBarcode.size() )
                           {
                              deliverBarcodeData( *params, sBarcode );
                              sBarcode = "" ;
                           }
                        }
                     } // process input buffer

                     if( 0 < sBarcode.size() )
                     {
                        pollRes = poll( &filedes, 1, 100 ); // 100 ms
                        if( 0 == pollRes )
                        {
//                         fprintf( stderr, "timeout\n" );
                           break;
                        }
                        else if( EINTR == errno )
                        {
//                         fprintf( stderr, "EINTR\n" );
                        }
                        else if( 0 == errno )
                        {
//                         fprintf( stderr, "bcPoll huh? %m\n" );
                        }
                        else
                        {
                           fprintf( stderr, "bcPoll err %m\n" );
                           break;
                        }
                     } // tail-end data, wait for timeout
                  }
                  else
                  {
                     fprintf( stderr, "null read\n" );
                  }
               } // until timeout or terminated end of input
               
               if( 0 < sBarcode.size() )
                  deliverBarcodeData( *params, sBarcode ); // un-terminated end of data

            } // terminator
            else
            {
               std::string sBarcode ;

               while( 1 )
               {
                  char inBuf[256];
                  int numRead = read( params->fd_, inBuf, sizeof( inBuf ) - 1 );
                  if( 0 < numRead )
                  {
                     inBuf[numRead] = '\0' ;
                     sBarcode += inBuf ;

                     pollRes = poll( &filedes, 1, 10 ); // 10 ms
                     if( 0 == pollRes )
                     {
//                        fprintf( stderr, "timeout\n" );
                        break;
                     }
                     else if( EINTR == errno )
                     {
//                        fprintf( stderr, "EINTR\n" );
                     }
                     else if( 0 == errno )
                     {
//                        fprintf( stderr, "bcPoll huh? %m\n" );
                     }
                     else
                     {
                        fprintf( stderr, "bcPoll err %m\n" );
                        break;
                     }
                  }
                  else
                  {
                     fprintf( stderr, "null read\n" );
                  }
               } // until nothing more to read
               
               if( 0 < sBarcode.size() )
                  deliverBarcodeData( *params, sBarcode );

            } // timed out reads

         } // have something to read
         else if( 0 > pollRes )
         {
            fprintf( stderr, ":bcpoll:%m\n" );
            break;
         }
         else
         {
         } // 
      }
   }
   else
      fprintf( stderr, "tcsetattr %m\n" );

   return 0 ;
}

static void jsBarcodeReaderFinalize(JSContext *cx, JSObject *obj);

JSClass jsBarcodeReaderClass_ = {
  "barcodeReader",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsBarcodeReaderFinalize,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

//
// constructor for the scanner object
//
static JSBool jsBarcodeReader( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   JSString *sDevice = JSVAL_TO_STRING( argv[0] );   
   if( ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) )
       && 
       ( 1 <= argc )
       && 
       ( 2 >= argc )
       &&
       ( 0 != ( sDevice = JSVAL_TO_STRING( argv[0] ) ) ) )
   {
      obj = JS_NewObject( cx, &jsBarcodeReaderClass_, NULL, NULL );
   
      if( obj )
      {
         *rval = OBJECT_TO_JSVAL(obj); // root
         JS_SetPrivate( cx, obj, 0 );

         JS_DefineProperty( cx, obj, "deviceName", argv[0], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );

         char terminator = '\0' ;

         if( 2 == argc )
         {
            JSString *sTerminator = JSVAL_TO_STRING( argv[1] );
            if( sTerminator && ( 1 == JS_GetStringLength( sTerminator ) ) )
            {
               JS_DefineProperty( cx, obj, "terminator", argv[1], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               terminator = *JS_GetStringBytes( sTerminator );
            }
            else
               JS_ReportError( cx, "barcodeReader terminator must be single-character string" );
         } // have terminator property
         
         char const *deviceName = JS_GetStringBytes( sDevice );
         int const fd = open( deviceName, O_RDWR | O_NOCTTY );
         if( 0 <= fd )
         {
            bcrParams_t *params = new bcrParams_t ;

            params->object_       = *rval ;
            params->scope_        = obj ;
            params->fd_           = fd ;
            params->terminator_   = terminator ;
            params->outputDelay_  = 0 ;
            JS_AddRoot( cx, &params->object_ );

            int const create = pthread_create( &params->threadHandle_, 0, barcodeThread, params );
            if( 0 == create )
            {
               JS_SetPrivate( cx, obj, params );
            }
            else
            {
               JS_ReportError( cx, "%s creating barcode reader thread\n", strerror( errno ) );
               close( fd );
               delete params ;
            }
         }
         else
            JS_ReportError( cx, "%s opening %s\n", strerror( errno ), deviceName );
      }
      else
      {
         JS_ReportError( cx, "allocating barcodeReader\n" );
         *rval = JSVAL_FALSE ;
      }
      
      return JS_TRUE;
   }
   else
      JS_ReportError( cx, "Usage : myVar = new barcodeReader( '/dev/ttyS2', '\r' );" );
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
jsSend( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      if( 1 == argc )
      {
         JSString *jsMsg = JSVAL_TO_STRING( argv[0] );
         char * p = JS_GetStringBytes( jsMsg );
         int len = JS_GetStringLength( jsMsg );
         int numWritten;
         if (params->outputDelay_==0)
            numWritten = write( params->fd_, p, len );
         else {
            numWritten=0;
            while (len) {
               usleep(params->outputDelay_);
               numWritten += write( params->fd_, p, 1 );
               p++;
               len--;
            }
         }
         *rval = INT_TO_JSVAL( numWritten );
      }
      else
         JS_ReportError( cx, "Usage: sendToScanner( \'string\' );" );
   }

   return JS_TRUE ;
}

static JSBool
jsFake( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      if( ( 2 == argc ) 
          && 
          JSVAL_IS_STRING( argv[0] )
          &&
          JSVAL_IS_INT( argv[1] ) )
      {
         jsval onbarcode ;
         if( JS_GetProperty( execContext_, params->scope_, "onBarcode", &onbarcode ) )
         {
            jsval bcv[2] = { argv[0], argv[1] };
            queueSource( params->scope_, onbarcode, "onBarcode", 2, bcv );
         }
         else
            printf( "no barcode handler\n" );

         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "Usage: barcodeScanner.fake( 'string', barcodeScanner.i2of5 );" );
   }

   return JS_TRUE ;
}

static JSBool
jsClose( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      JS_SetPrivate( cx, obj, 0 );
      JS_RemoveRoot( cx, &params->object_ );

      if( 0 <= params->fd_ )
      {
         printf( "closing file\n" );
         close( params->fd_ );
         params->fd_ = -1 ;
      }
      
      pthread_t thread = params->threadHandle_ ;
      if( thread )
      {
         printf( "shutting down thread\n" );
         params->threadHandle_ = 0 ;
         pthread_cancel( thread );
         void *exitStat ;
         pthread_join( thread, &exitStat );
      }
   }
   
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static void jsBarcodeReaderFinalize(JSContext *cx, JSObject *obj)
{
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      printf( "params %p\n", params );
      jsval args[1];
      jsval rval ;
      jsClose( cx, obj, 0, args, &rval );
   } // have button data
}

static JSBool
jsGetBaud( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      struct termios oldTermState;
      tcgetattr(params->fd_,&oldTermState);
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
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE ;
}

static JSBool
jsSetBaud( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
   
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
            tcgetattr(params->fd_,&termState);
            cfsetispeed( &termState, baudIdx );
            cfsetospeed( &termState, baudIdx );
         
            int result = tcsetattr( params->fd_, TCSANOW, &termState );
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
   }

   return JS_TRUE ;
}

static JSBool
jsGetParity( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      struct termios oldTermState;
      tcgetattr(params->fd_,&oldTermState);
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
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static JSBool
jsSetParity( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
      {
         struct termios termState;
         tcgetattr(params->fd_,&termState);
         
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
            int result = tcsetattr( params->fd_, TCSANOW, &termState );
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
   }
   return JS_TRUE ;
}

static JSBool
jsGetBits( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      struct termios oldTermState;
      tcgetattr(params->fd_,&oldTermState);
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
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static unsigned csLengthMasks_[] = {
   CS5,
   CS6,
   CS7,
   CS8
};

static JSBool
jsSetBits( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
      {
         int const bits = JSVAL_TO_INT( argv[0] );
         if( ( 5 <= bits ) && ( 8 >= bits ) )
         {
            struct termios termState;
            tcgetattr(params->fd_,&termState);
            
            termState.c_cflag &= ~CSIZE ;
            termState.c_cflag |= csLengthMasks_[bits-5];
         
            int result = tcsetattr( params->fd_, TCSANOW, &termState );
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
   }
   return JS_TRUE ;
}


static JSBool
jsGetDelay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      *rval = INT_TO_JSVAL( params->outputDelay_ );
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static JSBool
jsSetDelay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   bcrParams_t *params = (bcrParams_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
      {
         int const delay = JSVAL_TO_INT( argv[0] );
         if( ( 0 <= delay ) && ( delay <= 500000 ) )
         {
            params->outputDelay_ = delay;
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "must be 0-500000" );
      }
      else
         JS_ReportError( cx, "Usage: scanner.setBits( 0-500000);" );
   }

   return JS_TRUE ;
}

static JSFunctionSpec barcodeReader_methods[] = {
   { "send",            jsSend,      0,0,0 },
   { "fake",            jsFake,      0,0,0 },
   { "close",           jsClose,     0,0,0 },
   { "getBaud",         jsGetBaud,   0,0,0 },
   { "setBaud",         jsSetBaud,   0,0,0 },
   { "getParity",       jsGetParity, 0,0,0 },
   { "setParity",       jsSetParity, 0,0,0 },
   { "getBits",         jsGetBits,   0,0,0 },
   { "setBits",         jsSetBits,   0,0,0 },
   { "getDelay",        jsGetDelay,  0,0,0 },
   { "setDelay",        jsSetDelay,  0,0,0 },
   { 0 }
};

static pthread_t threadHandle_ = 0 ;

enum bcrTinyId {
   BCR_NOSYM,      // unknown symbology
   BCR_UPC,        // UPC
   BCR_I2OF5,      // Interleaved 2 of 5
   BCR_CODE39,     // Code 39 (alpha)
   BCR_CODE128,    // Code 128
   BCR_EAN,        // European Article Numbering System
   BCR_EAN128,     //    "        "        "        "
   BCR_CODABAR,    // 
   BCR_CODE93,     // 
   BCR_PHARMACODE, // 
   BCR_NUMSYMBOLOGIES
};

static char const *const bcrSymNames_[] = {
   "unknownSym",
   "upc",
   "i2of5",
   "code39",
   "code128",
   "ean",
   "ean128",
   "codabar",
   "code93",
   "pharmacode"
};

static JSPropertySpec bcReaderProperties_[] = {
  {0,0,0}
};

bool initJSBarcode( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsBarcodeReaderClass_,
                                  jsBarcodeReader, 1,
                                  bcReaderProperties_, 
                                  barcodeReader_methods,
                                  0, 0 );
   if( rval )
   {
      JSObject *symTable = JS_NewArrayObject( cx, 0, NULL );

      // root it
      if( JS_DefineProperty( cx, rval, "symbologyNames", OBJECT_TO_JSVAL( symTable ),
                             NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT ) )
      {
         for( unsigned i = BCR_NOSYM ; i < BCR_NUMSYMBOLOGIES ; i++ )
         {
            if( !JS_DefinePropertyWithTinyId( cx, rval, bcrSymNames_[i], i, INT_TO_JSVAL( i ), 
                                              NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT ) )
            {
               JS_ReportError( cx, "Defining barcodeReader.%s", bcrSymNames_[i] );
            }
            
            JSString *sName = JS_NewStringCopyZ( cx, bcrSymNames_[i] );
            if( sName )
            {
               if( !JS_DefineElement( cx, symTable, i, STRING_TO_JSVAL( sName ), NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT ) )
                  JS_ReportError( cx, "defining symbologyNames[%d]:%s\n", i, bcrSymNames_[i] );
            }
         }
      }
      else
         JS_ReportError( cx, "defining symbology names" );

      return true ;
   }
   else
      JS_ReportError( cx, "initializing scanner class" );

   return false ;
}

