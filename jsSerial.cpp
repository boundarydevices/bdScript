/*
 * Module jsSerial.cpp
 *
 * This module defines the initialization routine for the
 * Javascript serialPort class as declared in jsSerial.h
 *
 *
 * Change History : 
 *
 * $Log: jsSerial.cpp,v $
 * Revision 1.1  2004-03-27 20:24:35  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "jsSerial.h"

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
#include "serialPoll.h"
#include "baudRate.h"

static JSObject *spProto = NULL ;

class jsPort_t : public serialPoll_t {
public:
   jsPort_t( pollHandlerSet_t &set,
             char const       *devName,
             int               baud,
             int               databits,
             char              parity,
             int               outDelay,        // inter-character delay on output
             char              terminator,      // end-of-line char
             unsigned          inputTimeout,
             jsval             object,
             JSObject          *scope );
   virtual ~jsPort_t( void );

   virtual void onChar( void );
   virtual void onLineIn( void );
   
   jsval     object_ ;
   JSObject *scope_ ;
};


jsPort_t :: jsPort_t
   ( pollHandlerSet_t &set,
     char const       *devName,
     int               baud,
     int               databits,
     char              parity,
     int               outDelay,        // inter-character delay on output
     char              terminator,      // end-of-line char
     unsigned          inputTimeout,
     jsval             object,
     JSObject          *scope )
   : serialPoll_t( set, devName, baud, databits, parity, outDelay, terminator, inputTimeout )
   , object_( object )
   , scope_( scope )
{
   JS_AddRoot( execContext_, &object_ );
   JS_SetPrivate( execContext_, scope_, this );
}

jsPort_t :: ~jsPort_t( void )
{
   JS_SetPrivate( execContext_, scope_, 0 );
   JS_RemoveRoot( execContext_, &object_ );
}

void jsPort_t :: onChar( void )
{
   jsval handler ;
   if( JS_GetProperty( execContext_, scope_, "onChar", &handler ) 
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( execContext_, handler ) ) )
   {
      executeCode( scope_, handler, "onChar", 0, 0 );
   } // have handler
   else
   {
      printf( "no raw data handler\n" );
      // clear input
      std::string sData ;
      read(sData);
   } // no handler defined
}

void jsPort_t :: onLineIn( void )
{
   jsval handler ;
   if( JS_GetProperty( execContext_, scope_, "onLineIn", &handler ) 
       &&
       ( JSTYPE_FUNCTION == JS_TypeOfValue( execContext_, handler ) ) )
   {
      executeCode( scope_, handler, "onLineIn", 0, 0 );
   } // have handler
   else
   {
      printf( "no line input handler\n" );
      // clear input
      std::string sData ;
      while( readln( sData ) )
         ;
   } // no handler defined, purge input
}

static void jsSerialPortFinalize(JSContext *cx, JSObject *obj);

JSClass jsSerialPortClass_ = {
  "serialPort",
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsSerialPortFinalize,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

//
// constructor for the scanner object
//
static JSBool jsSerialPort( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   JSString *sDevice = JSVAL_TO_STRING( argv[0] );   
   if( ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) )
       && 
       ( 1 <= argc )
       && 
       ( 0 != ( sDevice = JSVAL_TO_STRING( argv[0] ) ) ) )
   {
      JSObject *portObj = JS_NewObject( cx, &jsSerialPortClass_, spProto, obj );
   
      if( portObj )
      {
         *rval = OBJECT_TO_JSVAL(portObj); // root
         JS_SetPrivate( cx, portObj, 0 );

         JS_DefineProperty( cx, portObj, "deviceName", argv[0], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );

         char terminator = '\0' ;
         unsigned inputTimeout = 0 ;

         if( 2 <= argc )
         {
            JSString *sTerminator = JSVAL_TO_STRING( argv[1] );
            if( sTerminator && ( 1 == JS_GetStringLength( sTerminator ) ) )
            {
               JS_DefineProperty( cx, portObj, "terminator", argv[1], 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               terminator = *JS_GetStringBytes( sTerminator );
            }
            else
               JS_ReportError( cx, "serialPort terminator must be single-character string" );
            if( 3 <= argc )
            {
               if( JSVAL_IS_INT( argv[2] ) )
               {
                  inputTimeout = (unsigned)JSVAL_TO_INT( argv[2] );
               }
               else
                  JS_ReportError( cx, "input timeout must be an integer" );
            } // input timeout property
         } // have terminator property

         (void )new jsPort_t( pollHandlers_, JS_GetStringBytes( sDevice ),
                              9600, 8, 'N', 0, terminator, inputTimeout, *rval, portObj );
      }
      else
      {
         JS_ReportError( cx, "allocating serialPort\n" );
         *rval = JSVAL_FALSE ;
      }
      
      return JS_TRUE;
   }
   else
      JS_ReportError( cx, "Usage : myVar = new serialPort( '/dev/ttyS2', '\r' );" );
}

static JSBool
jsSend( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsPort_t *params = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
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
jsClose( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
      delete port ;
   
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static void jsSerialPortFinalize(JSContext *cx, JSObject *obj)
{
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
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
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
      struct termios oldTermState;
      tcgetattr(port->getFd(),&oldTermState);
      unsigned short baudIdx = cfgetispeed(&oldTermState);
      unsigned baud ;
      if( !constToBaudRate( baudIdx, baud ) )
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
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
   
      if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
      {
         unsigned brConst ;
         unsigned const bps = (unsigned)JSVAL_TO_INT( argv[0] );
         if( baudRateToConst( bps, brConst ) )
         {
            struct termios termState;
            tcgetattr(port->getFd(),&termState);
            cfsetispeed( &termState, brConst );
            cfsetospeed( &termState, brConst );
         
            int result = tcsetattr( port->getFd(), TCSANOW, &termState );
            if( 0 == result )
            {
               *rval = JSVAL_TRUE ;
            }
            else
               perror( "setBaud" );
         }
         else
            JS_ReportError( cx, "Invalid baud rate %u", bps );
      }
      else
         JS_ReportError( cx, "Usage: scanner.setBaud( 9600 );" );
   }

   return JS_TRUE ;
}

static JSBool
jsGetParity( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
      struct termios oldTermState;
      tcgetattr(port->getFd(),&oldTermState);
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
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
      if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
      {
         struct termios termState;
         tcgetattr(port->getFd(),&termState);
         
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
            int result = tcsetattr( port->getFd(), TCSANOW, &termState );
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
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
      struct termios oldTermState;
      tcgetattr(port->getFd(),&oldTermState);
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
   
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
      if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
      {
         int const bits = JSVAL_TO_INT( argv[0] );
         if( ( 5 <= bits ) && ( 8 >= bits ) )
         {
            struct termios termState;
            tcgetattr(port->getFd(),&termState);
            
            termState.c_cflag &= ~CSIZE ;
            termState.c_cflag |= csLengthMasks_[bits-5];
         
            int result = tcsetattr( port->getFd(), TCSANOW, &termState );
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
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
      *rval = INT_TO_JSVAL( port->outputDelay() );
   }
   else
      *rval = JSVAL_FALSE ;

   return JS_TRUE ;
}

static JSBool
jsSetDelay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
      if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
      {
         int const delay = JSVAL_TO_INT( argv[0] );
         if( ( 0 <= delay ) && ( delay <= 500000 ) )
         {
            port->setOutputDelay( delay );
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

static JSBool
jsRead( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
      std::string s ;
      if( port->read( s ) )
      {
         JSString *const sData = JS_NewStringCopyN( cx, s.c_str(), s.size() );
         if( sData )
         {
            *rval = STRING_TO_JSVAL( sData );
         }
         else
            JS_ReportError( cx, "allocating serialPort raw data" );
         
      }
   }

   return JS_TRUE ;
}

static JSBool
jsReadln( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   jsPort_t *port = (jsPort_t *)JS_GetInstancePrivate( cx, obj, &jsSerialPortClass_, NULL );
   if( port )
   {
      std::string s ;
      if( port->readln( s ) )
      {
         JSString *const sData = JS_NewStringCopyN( cx, s.c_str(), s.size() );
         if( sData )
         {
            *rval = STRING_TO_JSVAL( sData );
         }
         else
            JS_ReportError( cx, "allocating serialPort line" );
         
      }
   }

   return JS_TRUE ;
}

static JSFunctionSpec serialPort_methods[] = {
   { "send",            jsSend,      0,0,0 },
   { "close",           jsClose,     0,0,0 },
   { "read",            jsRead,      0,0,0 },
   { "readln",          jsReadln,    0,0,0 },
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


static JSPropertySpec spProperties_[] = {
  {0,0,0}
};

bool initJSSerial( JSContext *cx, JSObject *glob )
{
   spProto = JS_InitClass( cx, glob, NULL, &jsSerialPortClass_,
                           jsSerialPort, 1, spProperties_, serialPort_methods,
                           0, 0 );
   if( spProto )
   {
      JS_AddRoot( cx, &spProto );
      return true ;
   }
   else
      JS_ReportError( cx, "initializing scanner class" );

   return false ;
}

