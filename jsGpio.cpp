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
#include <sys/poll.h>
#include "codeQueue.h"
#include "jsGlobals.h"
#include "linux/pxa-gpio.h"
#include <fcntl.h>

static int fdAmber =0;
static int fdRed =0;
static int fdGreen =0;
static int fdTurnstile =0;
static int fdDoorlock =0;
static jsval sFeedbackHandlerLow[2] = { JSVAL_VOID, JSVAL_VOID };
static jsval sFeedbackHandlerHigh[2] = { JSVAL_VOID, JSVAL_VOID };
static JSObject *feedbackHandlerScope = 0 ;
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

static char const * const devNames[2] = {
   "/dev/Feedback",
   "/dev/Feedback2"
};

static void* FeedbackThread( void *arg )
{
   pollfd pfds[2];
   for( int i = 0 ; i < 2 ; i++ )
   {
      char const *devName =  devNames[i];
      pfds[i].events = POLLIN ;
      pfds[i].fd = open( devName, O_RDONLY);
      if( pfds[i].fd < 0)
      {
         fprintf( stderr, "Error %s opening %s\n",strerror(errno), devName);
         while( 0 < i-- )
            close( pfds[i].fd );
         return 0 ;
      }
      fcntl( pfds[i].fd, F_SETFD, FD_CLOEXEC );
   }
   
   while( 1 )
   {
      int pResult = poll( pfds, 2, -1 );
      if( 0 < pResult )
      {
         for( int i = 0 ; i < 2 ; i++ )
         {
            if( POLLIN & pfds[i].revents )
            {
               char ch;
               int numRead ;
               if( (numRead = read( pfds[i].fd, &ch, 1)) >= 0)
               {
                  if ( numRead > 0)
                  {
                     if (feedbackHandlerScope != 0)
                     {
                        if (ch&1)
         	        {
                           if (sFeedbackHandlerHigh[i] != JSVAL_VOID)
         		      queueSource( feedbackHandlerScope, sFeedbackHandlerHigh[i], "onFeedbackHigh" );
                        }
                        else
                        {
         	           if (sFeedbackHandlerLow[i] != JSVAL_VOID)
         		      queueSource( feedbackHandlerScope, sFeedbackHandlerLow[i], "onFeedbackLow" );
                        }
                     }
                  }
                  else
                  {
                     fprintf( stderr, "feedback null read\n" );
                     break;
                  }
               }
               else
                  break;
            }
         } // for each device
      }
      else
      {
         fprintf( stderr, "feedback pollerr %m\n" );
         break;
      }
   }
   
   for( int i = 0 ; i < 2 ; i++ )
      close( pfds[i].fd );
   
   return 0 ;
}

static JSBool jsOnFeedback(int which,jsval psFeed[], JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
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
         psFeed[feedbackId] = argv[1];
         *rval = JSVAL_TRUE ;
         if (feedbackThreadHandle==0)
            if (pthread_create( &feedbackThreadHandle, 0, FeedbackThread, 0 )!=0)
               feedbackThreadHandle = 0;
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
   return jsOnFeedback(0,sFeedbackHandlerLow,cx,obj,argc,argv,rval );
}
static JSBool jsOnFeedbackHigh( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   return jsOnFeedback(0,sFeedbackHandlerHigh,cx,obj,argc,argv,rval );
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
   for( int i = 0 ; i < 2 ; i++ )
   {
      JS_AddRoot( cx, &sFeedbackHandlerLow[i]);
      JS_AddRoot( cx, &sFeedbackHandlerHigh[i]);
   }
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

   for( int i = 0 ; i < 2 ; i++ )
   {
      JS_RemoveRoot( execContext_, &sFeedbackHandlerLow[i]);
      JS_RemoveRoot( execContext_, &sFeedbackHandlerHigh[i]);
   }
}
