/*
 * Module jsVolume.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: jsVolume.cpp,v $
 * Revision 1.2  2002-12-15 00:11:42  ericn
 * -removed debug msgs
 *
 * Revision 1.1  2002/11/14 13:14:50  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsVolume.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include "audioQueue.h"

static JSBool
jsGetVolume( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   int const dspFd = getDspFd();
   if( 0 <= dspFd )
   {
      int vol;
      if( 0 <= ioctl( dspFd, SOUND_MIXER_READ_VOLUME, &vol)) 
      {
         *rval = INT_TO_JSVAL( vol ) & 0xFF ;
      }
      else
         JS_ReportError( cx, "Error reading volume" );
   }
   else
      JS_ReportError( cx, "Error %d:%s opening dsp device", dspFd, strerror(errno) );
   
   return JS_TRUE ;
}

static JSBool
jsSetVolume( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   unsigned volParam ;

   *rval = JSVAL_FALSE ;

   if( ( 1 == argc ) 
       && 
       JSVAL_IS_INT( argv[0] ) 
       &&
       ( 100 > (unsigned)( volParam = JSVAL_TO_INT(argv[0]) ) ) )
   {
      int const dspFd = getDspFd();
      if( 0 <= dspFd )
      {
         int const vol = (volParam << 8) | volParam ;
         if( 0 <= ioctl( dspFd, SOUND_MIXER_WRITE_VOLUME, &vol)) 
         {
            printf( "volume == 0x%x\n", vol );
            *rval = INT_TO_JSVAL( vol ) & 0xFF ;
         }
         else
            JS_ReportError( cx, "Error setting volume" );
      }
      else
         JS_ReportError( cx, "Error %d:%s opening dsp device", dspFd, strerror(errno) );
   }
   else
      JS_ReportError( cx, "Usage : setVolume( int [0..99] );\n" );
   
   return JS_TRUE ;
}

static JSFunctionSpec _functions[] = {
    {"getVolume",         jsGetVolume,      0 },
    {"setVolume",         jsSetVolume,      1 },
    {0}
};


bool initJSVolume( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, _functions);
}


