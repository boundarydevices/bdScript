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
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include "codeQueue.h"
#include "jsGlobals.h"
#include "linux/pxa-gpio.h"

static int fdAmber =0;
static int fdRed =0;
static int fdGreen =0;
static int fdTurnstile =0;
static int fdDoorlock =0;
static jsval sFeedbackHandlerLow = JSVAL_VOID;
static jsval sFeedbackHandlerHigh = JSVAL_VOID;
static JSObject *feedbackHandlerScope;
static pthread_t feedbackThreadHandle = 0 ;

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
      if ( (fd > 0) && (write( fd, &ledState, 1) >= 0) )
      {
         printf( "%s = %i\n", device,state);
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


static void* FeedbackThread( void *arg )
{
   int fd = open( "/dev/Feedback", O_RDONLY);
   if (fd<0) fprintf( stderr, "Error %d:%s opening /dev/feedback\n",fd,strerror(errno));
   else
   {
      char ch;
      int  numRead;
      while ( (numRead = read( fd, &ch, 1)) >= 0)
      {
         if ( numRead > 0)
         {
            printf( "feedback %02x", (unsigned char)ch );
            if (feedbackHandlerScope != 0)
            {
               if (ch&1)
	       {
	          if (sFeedbackHandlerHigh != JSVAL_VOID)
		    queueSource( feedbackHandlerScope, sFeedbackHandlerHigh, "onFeedbackHigh" );
               }
               else
	       {
	          if (sFeedbackHandlerLow != JSVAL_VOID)
		    queueSource( feedbackHandlerScope, sFeedbackHandlerLow, "onFeedbackLow" );
               }
            }
         }
         else
         {
            fprintf( stderr, "feedback null read\n" );
            break;
         }
         
      }
      close(fd);
   }
   return 0 ;
}

static JSBool jsOnFeedback(jsval* psFeed, JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if ( ( argc == 1) && JSVAL_IS_STRING( argv[0] ) )
   {
      *psFeed = argv[0];
      *rval = JSVAL_TRUE ;
      if (feedbackThreadHandle==0)
         if (pthread_create( &feedbackThreadHandle, 0, FeedbackThread, 0 )!=0)
            feedbackThreadHandle = 0;
   }
   return JS_TRUE ;
}
static JSBool jsOnFeedbackLow( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsOnFeedback(&sFeedbackHandlerLow,cx,obj,argc,argv,rval );
}
static JSBool jsOnFeedbackHigh( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsOnFeedback(&sFeedbackHandlerHigh,cx,obj,argc,argv,rval );
}
static JSBool jsGetDebounce( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval, int ioctlVal )
{
   *rval = JSVAL_FALSE ;
   int const fd = open( "/dev/Feedback", O_RDONLY);
   if( 0 <= fd )
   {
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
   JS_AddRoot( cx, &sFeedbackHandlerLow);
   JS_AddRoot( cx, &sFeedbackHandlerHigh);
   feedbackHandlerScope = glob ;
   return JS_DefineFunctions( cx, glob, gpio_functions);
}

void shutdownGpio()
{
   if (fdAmber>0)     { close( fdAmber );     fdAmber=0;}
   if (fdRed>0)       { close( fdRed );       fdRed=0;}
   if (fdGreen>0)     { close( fdGreen );     fdGreen=0;}
   if (fdDoorlock>0)  { close( fdDoorlock );  fdDoorlock=0;}
   if (fdTurnstile>0) { close( fdTurnstile ); fdTurnstile=0;}
   if (feedbackThreadHandle)
   {
      pthread_cancel(feedbackThreadHandle);
      void *exitStat ;
      pthread_join( feedbackThreadHandle, &exitStat );
   }

   JS_RemoveRoot( execContext_, &sFeedbackHandlerLow);
   JS_RemoveRoot( execContext_, &sFeedbackHandlerHigh);
}
