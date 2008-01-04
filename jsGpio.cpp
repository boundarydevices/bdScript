/*
 * Module jsGpio.cpp
 *
 * This module defines ...
 *
 *
 * Change History :
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsGpio.h"
#include "config.h"

#ifndef KERNEL_PXA_GPIO
#error No PXA GPIO support in kernel
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/poll.h>
#include "codeQueue.h"
#include "jsGlobals.h"
#include "linux/pxa-gpio.h"
#include <fcntl.h>
#include "pollHandler.h"


static char const * const devNames[2] = {
   "/dev/Feedback",
   "/dev/Feedback2"
};

class gpioHandler_t : public pollHandler_t {
public:
   gpioHandler_t( JSContext    *cx,
                  unsigned char which,
                  JSObject     *scope );
   ~gpioHandler_t( void );

   void setHandler( bool high, jsval handler ){ handlers_[high] = handler ; }
   virtual void onDataAvail( void );     // POLLIN

private:
   JSContext *cx_ ;
   JSObject  *scope_ ;
   jsval      handlers_[2];
};

gpioHandler_t :: gpioHandler_t
   ( JSContext    *cx,
    unsigned char which,
     JSObject     *scope )
   : pollHandler_t( open( devNames[which], O_RDONLY ), pollHandlers_ )
   , cx_( cx )
   , scope_( scope )
{
   handlers_[0] = handlers_[1] = JSVAL_VOID ;
   fcntl( getFd(), F_SETFD, FD_CLOEXEC );
   setMask( POLLIN );
   pollHandlers_.add( *this );
   JS_AddRoot( cx, &scope_ );
   JS_AddRoot( cx, &handlers_[0] );
   JS_AddRoot( cx, &handlers_[1] );
}

gpioHandler_t :: ~gpioHandler_t( void )
{
   JS_RemoveRoot( cx_, &scope_ );
   JS_RemoveRoot( cx_, &handlers_[0] );
   JS_RemoveRoot( cx_, &handlers_[1] );
   close();
}

void gpioHandler_t :: onDataAvail( void )
{
   char c ;
   if( 1 == read( getFd(), &c, 1 ) )
   {
      bool const high = ( 0 != ( c & 1 ) );
      if( JSVAL_VOID != handlers_[high] )
         queueSource( scope_, handlers_[high], "onFeedback" );
   }
   else
      perror( "gpioRead" );
}

static int fdAmber =0;
static int fdRed =0;
static int fdGreen =0;
static int fdTurnstile =0;
static int fdDoorlock =0;
static gpioHandler_t *handlers_[2] = {
   0, 0
};

static JSBool jsSetGpio( int &fd, char* device,JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{

   unsigned state;

   *rval = JSVAL_FALSE ;

   if ( ( argc == 1) &&
       JSVAL_IS_INT( argv[0] ) &&
       ( (unsigned)( state = JSVAL_TO_INT(argv[0]) )<=1 ) )
   {
      char ledState = (char)state;
      if (fd<=0)
      {
         char buffer[80];
	 memcpy(buffer,"/dev/",5);
	 memcpy(&buffer[5],device,strlen(device)+1);
         fd = open(buffer, O_WRONLY );
      }
      if ( (fd >= 0) && (write( fd, &ledState, 1) >= 0) )
      {
         fcntl( fd, F_SETFD, FD_CLOEXEC );
         *rval = JSVAL_TRUE;
      }
      else
         JS_ReportError( cx, "Error %d:%s setting %s to %i",fd,strerror(errno),device,state );
   }
   else
      JS_ReportError( cx, "Usage : set%s( [0,1] );\n",device );

   return JS_TRUE ;
}
static JSBool jsSetAmber( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsSetGpio(fdAmber,"Amber",cx,obj,argc,argv,rval);
}
static JSBool jsSetRed( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsSetGpio(fdRed,"Red",cx,obj,argc,argv,rval);
}
static JSBool jsSetGreen( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsSetGpio(fdGreen,"Green",cx,obj,argc,argv,rval);
}
static JSBool jsSetDoorlock( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsSetGpio(fdDoorlock,"Doorlock",cx,obj,argc,argv,rval);
}
static JSBool jsSetTurnstile( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsSetGpio(fdTurnstile,"Turnstile",cx,obj,argc,argv,rval);
}

static JSBool jsOnFeedback( bool high, JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if ( ( argc == 2) 
        && 
        JSVAL_IS_INT( argv[0] )
        && 
        JSVAL_IS_STRING( argv[1] ) )
   {
      int const feedbackId = JSVAL_TO_INT( argv[0] );
      if( ( 0 <= feedbackId ) && ( 2 > feedbackId ) )
      {
         if( 0 == handlers_[feedbackId] )
            handlers_[feedbackId] = new gpioHandler_t( cx, (unsigned char)feedbackId, obj );
         handlers_[feedbackId]->setHandler( high, argv[1] );
         *rval = JSVAL_TRUE ;
      }
      else
         JS_ReportError( cx, "invalid feedback id, use 0 or 1" );
   }
   else
      JS_ReportError( cx, "usage: onFeedback[Low|High]( [0|1], code )" );

   return JS_TRUE ;
}

static JSBool jsOnFeedbackLow( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsOnFeedback(false,cx,obj,argc,argv,rval );
}

static JSBool jsOnFeedbackHigh( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsOnFeedback(true,cx,obj,argc,argv,rval );
}

static JSBool jsGetDebounce( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval, int ioctlVal )
{
   *rval = JSVAL_FALSE ;
   int const fd = open( "/dev/Feedback", O_RDONLY);
   if( 0 <= fd )
   {
      fcntl( fd, F_SETFD, FD_CLOEXEC );
      int result = 0 ;
      if( 0 == ioctl( fd, ioctlVal, &result ) )
      {
         *rval = INT_TO_JSVAL( result );
      }
      else
         JS_ReportError( cx, "%m reading Feedback debounce\n" );

      close(fd);
   }
   else
      JS_ReportError( cx, "%m opening Feedback device" );

   return JS_TRUE ;
}
static JSBool jsSetDebounce( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval, int ioctlVal )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) && JSVAL_IS_INT( argv[0] ) )
   {
      int const fd = open( "/dev/Feedback", O_RDONLY);
      if( 0 <= fd )
      {
         fcntl( fd, F_SETFD, FD_CLOEXEC );
         int ms = JSVAL_TO_INT(argv[0]);
         if( 0 == ioctl( fd, ioctlVal, &ms ) )
         {
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "%m setting Feedback debounce\n" );
   
         close(fd);
      }
      else
         JS_ReportError( cx, "%m opening Feedback device" );
   }
   else
      JS_ReportError( cx, "usage: setFBxDebounce ms" );

   return JS_TRUE ;
}
static JSBool jsGetFBLowDB( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsGetDebounce( cx, obj, argc, argv, rval, GPIO_GET_DEBOUNCE_DELAY_LOW );
}
static JSBool jsSetFBLowDB( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsSetDebounce( cx, obj, argc, argv, rval, GPIO_SET_DEBOUNCE_DELAY_LOW );
}
static JSBool jsGetFBHighDB( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsGetDebounce( cx, obj, argc, argv, rval, GPIO_GET_DEBOUNCE_DELAY_HIGH );
}
static JSBool jsSetFBHighDB( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsSetDebounce( cx, obj, argc, argv, rval, GPIO_SET_DEBOUNCE_DELAY_HIGH );
}

static JSFunctionSpec gpio_functions[] = {
    {"setAmber",       jsSetAmber,       1 },
    {"setRed",         jsSetRed,         1 },
    {"setGreen",       jsSetGreen,       1 },
    {"setDoorlock",    jsSetDoorlock,    1 },
    {"setTurnstile",   jsSetTurnstile,   1 },
    {"onFeedbackLow",  jsOnFeedbackLow,  1 },
    {"onFeedbackHigh", jsOnFeedbackHigh, 1 },
    {"getFBLowDebounce", jsGetFBLowDB, 1 },
    {"setFBLowDebounce", jsSetFBLowDB, 1 },
    {"getFBHighDebounce", jsGetFBHighDB, 1 },
    {"setFBHighDebounce", jsSetFBHighDB, 1 },
    {0}
};


bool initJSGpio( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, gpio_functions);
}

void shutdownGpio()
{
   if (fdAmber>0)     { close( fdAmber );     fdAmber=0;}
   if (fdRed>0)       { close( fdRed );       fdRed=0;}
   if (fdGreen>0)     { close( fdGreen );     fdGreen=0;}
   if (fdDoorlock>0)  { close( fdDoorlock );  fdDoorlock=0;}
   if (fdTurnstile>0) { close( fdTurnstile ); fdTurnstile=0;}
}
