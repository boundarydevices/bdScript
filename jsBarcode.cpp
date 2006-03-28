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
 * Revision 1.17  2006-03-28 04:09:32  ericn
 * -cleanup
 *
 * Revision 1.16  2005/12/11 16:02:30  ericn
 * -
 *
 * Revision 1.15  2004/05/23 21:25:59  ericn
 * -make symbol names work off of barcodeReader class
 *
 * Revision 1.14  2004/05/07 13:32:34  ericn
 * -made internals external (and shareable)
 *
 * Revision 1.13  2003/12/28 00:36:03  ericn
 * -removed secondary thread
 *
 * Revision 1.12  2003/12/27 15:09:29  ericn
 * -keep and use prototype
 *
 * Revision 1.11  2003/07/03 13:59:21  ericn
 * -modified to close bc handle on exec
 *
 * Revision 1.10  2003/06/10 23:54:08  ericn
 * -added check on typeof( onData )
 *
 * Revision 1.9  2003/06/08 15:19:07  ericn
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
#include "barcodePoll.h"

static JSObject *bcReaderProto = NULL ;

class bcReader_t : public barcodePoll_t {
public:
   bcReader_t( pollHandlerSet_t &set,
               char const       *devName,
               int               baud,
               int               databits,
               char              parity,
               int               outDelay,        // inter-character delay on output
               char              terminator,      // end-of-barcode char
               jsval             object,
               JSObject          *scope );
   virtual ~bcReader_t( void );

   virtual void onBarcode( void );
   
   jsval     object_ ;
   JSObject *scope_ ;
};


bcReader_t :: bcReader_t
   ( pollHandlerSet_t &set,
     char const       *devName,
     int               baud,
     int               databits,
     char              parity,
     int               outDelay,        // inter-character delay on output
     char              terminator,      // end-of-barcode char
     jsval             object,
     JSObject          *scope )
   : barcodePoll_t( set, devName, baud, databits, parity, outDelay, terminator )
   , object_( object )
   , scope_( scope )
{
   JS_AddRoot( execContext_, &object_ );
   JS_SetPrivate( execContext_, scope_, this );
}

bcReader_t :: ~bcReader_t( void )
{
   JS_SetPrivate( execContext_, scope_, 0 );
   JS_RemoveRoot( execContext_, &object_ );

}

void bcReader_t :: onBarcode( void )
{
   jsval onData ;
   if( JS_GetProperty( execContext_, scope_, "onData", &onData ) 
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( execContext_, onData ) ) )
   {
      JSString *jsBarcode = JS_NewStringCopyZ( execContext_, getBarcode() );
      if( jsBarcode )
      {
         JS_AddRoot( execContext_, &jsBarcode );
         jsval bcv[1] = { STRING_TO_JSVAL( jsBarcode ) };
         executeCode( scope_, onData, "onData", 1, bcv );
         JS_RemoveRoot( execContext_, &jsBarcode );
      }
      else
         fprintf( stderr, "Error allocating barcode\n" );
   }
   else
      printf( "no barcode handler\n" );
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
      JSObject *readerObj = JS_NewObject( cx, &jsBarcodeReaderClass_, bcReaderProto, obj );
   
      if( readerObj )
      {
         *rval = OBJECT_TO_JSVAL(readerObj); // root
         JS_SetPrivate( cx, readerObj, 0 );

         JS_DefineProperty( cx, readerObj, "deviceName", argv[0], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );

         char terminator = '\0' ;

         if( 2 == argc )
         {
            JSString *sTerminator = JSVAL_TO_STRING( argv[1] );
            if( sTerminator && ( 1 == JS_GetStringLength( sTerminator ) ) )
            {
               JS_DefineProperty( cx, readerObj, "terminator", argv[1], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               terminator = *JS_GetStringBytes( sTerminator );
            }
            else
               JS_ReportError( cx, "barcodeReader terminator must be single-character string" );
         } // have terminator property

         (void )new bcReader_t( pollHandlers_, JS_GetStringBytes( sDevice ),
                                9600, 8, 'N', 0, terminator, *rval, readerObj );
      }
      else
      {
         JS_ReportError( cx, "allocating barcodeReader\n" );
         *rval = JSVAL_FALSE ;
      }
   }
   else
      JS_ReportError( cx, "Usage : myVar = new barcodeReader( '/dev/ttyS2', '\r' );" );
   
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
jsSend( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   bcReader_t *params = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( params )
   {
      if( 1 == argc )
      {
         JSString *jsMsg = JSVAL_TO_STRING( argv[0] );
         char * p = JS_GetStringBytes( jsMsg );
         int len = JS_GetStringLength( jsMsg );
         int numWritten;
         if (params->outputDelay()==0)
            numWritten = write( params->getFd(), p, len );
         else {
            numWritten=0;
            while (len) {
               usleep(params->outputDelay() );
               numWritten += write( params->getFd(), p, 1 );
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
   bcReader_t *params = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
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
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
      delete reader ;
   
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static void jsBarcodeReaderFinalize(JSContext *cx, JSObject *obj)
{
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
   {
      jsval args[1] = { 
         JSVAL_VOID 
      };
      jsval rval ;
      jsClose( cx, obj, 0, args, &rval );
   } // have button data
}

static JSBool
jsGetBaud( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
   {
      struct termios oldTermState;
      tcgetattr(reader->getFd(),&oldTermState);
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
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
   {
   
      if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
      {
         unsigned baudIdx ;
         bool haveBaud = false ;
         
         unsigned const baud = JSVAL_TO_INT( argv[0] );
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
            tcgetattr(reader->getFd(),&termState);
            cfsetispeed( &termState, baudIdx );
            cfsetospeed( &termState, baudIdx );
         
            int result = tcsetattr( reader->getFd(), TCSANOW, &termState );
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
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
   {
      struct termios oldTermState;
      tcgetattr(reader->getFd(),&oldTermState);
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
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
   {
      if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
      {
         struct termios termState;
         tcgetattr(reader->getFd(),&termState);
         
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
            int result = tcsetattr( reader->getFd(), TCSANOW, &termState );
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
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
   {
      struct termios oldTermState;
      tcgetattr(reader->getFd(),&oldTermState);
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
   
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
   {
      if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
      {
         int const bits = JSVAL_TO_INT( argv[0] );
         if( ( 5 <= bits ) && ( 8 >= bits ) )
         {
            struct termios termState;
            tcgetattr(reader->getFd(),&termState);
            
            termState.c_cflag &= ~CSIZE ;
            termState.c_cflag |= csLengthMasks_[bits-5];
         
            int result = tcsetattr( reader->getFd(), TCSANOW, &termState );
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
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
   {
      *rval = INT_TO_JSVAL( reader->outputDelay() );
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static JSBool
jsSetDelay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   bcReader_t *reader = (bcReader_t *)JS_GetInstancePrivate( cx, obj, &jsBarcodeReaderClass_, NULL );
   if( reader )
   {
      if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
      {
         int const delay = JSVAL_TO_INT( argv[0] );
         if( ( 0 <= delay ) && ( delay <= 500000 ) )
         {
            reader->setOutputDelay( delay );
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

char const *const bcrSymNames_[BCR_NUMSYMBOLOGIES] = {
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

enum {
   sym_unknownSym,    
   sym_upc,           
   sym_i2of5,         
   sym_code39,            
   sym_code128,           
   sym_ean,               
   sym_ean128,            
   sym_codabar,           
   sym_code93,            
   sym_pharmacode         
};

static JSPropertySpec bcReaderProperties_[] = {
   {bcrSymNames_[0], sym_unknownSym,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {bcrSymNames_[1], sym_upc,           JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {bcrSymNames_[2], sym_i2of5,         JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {bcrSymNames_[3], sym_code39,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {bcrSymNames_[4], sym_code128,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {bcrSymNames_[5], sym_ean,           JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {bcrSymNames_[6], sym_ean128,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {bcrSymNames_[7], sym_codabar,       JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {bcrSymNames_[8], sym_code93,        JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {bcrSymNames_[9], sym_pharmacode,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT, 0, 0 },
   {0,0,0,0,0}
};


bool initJSBarcode( JSContext *cx, JSObject *glob )
{
   bcReaderProto = JS_InitClass( cx, glob, NULL, &jsBarcodeReaderClass_,
                                 jsBarcodeReader, 1,
                                 bcReaderProperties_, 
                                 barcodeReader_methods,
                                 bcReaderProperties_, 0 );
   if( bcReaderProto )
   {
      JS_AddRoot( cx, &bcReaderProto );

      JSObject *ctor = JS_GetConstructor( cx, bcReaderProto );
      if( 0 != ctor )
      {
         JSObject *symTable = JS_NewArrayObject( cx, 0, NULL );
   
         // root it
         if( JS_DefineProperty( cx, ctor, "symbologyNames", OBJECT_TO_JSVAL( symTable ),
                                NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT ) )
         {
            for( unsigned i = BCR_NOSYM ; i < BCR_NUMSYMBOLOGIES ; i++ )
            {
               if( !JS_DefinePropertyWithTinyId( cx, ctor, bcrSymNames_[i], i, INT_TO_JSVAL( i ), 
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
            return true ;
         }
         else
            JS_ReportError( cx, "defining symbology names" );
      }
      else
         JS_ReportError( cx, "getting barcodeReader constructor" );

   }
   else
   {
      JS_ReportError( cx, "initializing scanner class" );
   }

   return false ;
}

