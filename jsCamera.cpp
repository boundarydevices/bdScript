/*
 * Module jsCamera.cpp
 *
 * This module defines the Javascript Camera
 * class as described in jsCamera.h
 *
 *
 * Change History : 
 *
 * $Log: jsCamera.cpp,v $
 * Revision 1.6  2005-11-27 15:35:21  ericn
 * -remove debug msg
 *
 * Revision 1.5  2005/11/06 00:49:28  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.4  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.3  2003/06/04 02:56:32  ericn
 * -modified to allocate camera even if unplugged
 *
 * Revision 1.2  2003/04/21 01:25:05  ericn
 * -modified to use mmap
 *
 * Revision 1.1  2003/04/21 00:25:19  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


#include "jsCamera.h"
#include "fbDev.h"
#include <string.h>
#include "js/jsstddef.h"
#include "js/jscntxt.h"
#include "js/jsapi.h"
#include "js/jslock.h"
#include "jsImage.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/videodev.h>
#include <pthread.h>

enum jsCamera_tinyId {
   CAMERA_DEVICE,
   CAMERA_NAME,
   CAMERA_TYPE,
   CAMERA_MINWIDTH,
   CAMERA_MAXWIDTH,
   CAMERA_MINHEIGHT,
   CAMERA_MAXHEIGHT,
   CAMERA_DISPLAYX,
   CAMERA_DISPLAYY,
   CAMERA_WIDTH, 
   CAMERA_HEIGHT, 
   CAMERA_COLORDEPTH,
   CAMERA_COLOR
};

JSClass jsCameraClass_ = {
  "Camera",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec cameraProperties_[] = {
  {0,0,0}
};

static pthread_mutex_t const pthreadMutexDefault = PTHREAD_MUTEX_INITIALIZER ; 

struct cameraThreadParams_t {
   pthread_t tHandle_ ;
   int            fd_ ;
   unsigned       x_ ;
   unsigned       y_ ;
   unsigned       w_ ;
   unsigned       h_ ;

   //
   // fields used for grabs: 
   //    grabber clears imgBuf_, sets wantImage_, and waits on condition
   //    reader allocates and fills in imgBuf_, then posts condition
   //
   unsigned char    *imgBuf_ ;
   bool volatile     wantImage_ ;
   pthread_mutex_t   mutex_ ;
   pthread_cond_t    condition_ ;

   cameraThreadParams_t( unsigned x, unsigned y,
                         unsigned w, unsigned h )
      : x_( x ),
        y_( y ),
        w_( w ),
        h_( h ),
        imgBuf_( 0 ),
        wantImage_( false ),
        mutex_( pthreadMutexDefault )
   {
      pthread_cond_init( &condition_, 0 );
   }

   ~cameraThreadParams_t( void )
   {
      pthread_mutex_destroy( &mutex_ ); 
      pthread_cond_destroy( &condition_ );
   }

private:
   cameraThreadParams_t( cameraThreadParams_t const & ); // no copies
};

static JSBool
jsGrab( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cameraThreadParams_t *const params = (cameraThreadParams_t *)JS_GetPrivate( cx, obj );
   if( params )
   {
      pthread_mutex_lock( &params->mutex_ );
      params->imgBuf_ = 0 ;
      params->wantImage_ = true ;
      while( 0 == params->imgBuf_ )
      {
         pthread_cond_wait( &params->condition_, &params->mutex_ );
      }

      unsigned char const * const rgb = params->imgBuf_ ;
      params->imgBuf_ = 0 ;

      pthread_mutex_unlock( &params->mutex_ );

      JSObject *rImage = JS_NewObject( cx, &jsImageClass_, 0, 0 );
      if( rImage )
      {
         JS_DefineProperty( cx, rImage, "width",
                            INT_TO_JSVAL( params->w_ ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         JS_DefineProperty( cx, rImage, "height", 
                            INT_TO_JSVAL( params->h_ ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         unsigned const pixBytes = params->w_*params->h_*sizeof(unsigned short);
         unsigned short * const pixels = (unsigned short *)JS_malloc( cx, pixBytes );
         if( pixels )
         {
            unsigned offs = 0 ;
            for( unsigned y = 0 ; y < params->h_ ; y++ )
            {
               for( unsigned x = 0 ; x < params->w_ ; x++, offs += 3 )
               {
                  pixels[(y*params->w_)+x] = fbDevice_t::get16( rgb[offs+2], rgb[offs+1], rgb[offs] );
               } // for each column
            } // for each row requested

            JSString *sPix = JS_NewString( cx, (char *)pixels, pixBytes );
            if( sPix )
            {
               JS_DefineProperty( cx, rImage, "pixBuf", 
                                  STRING_TO_JSVAL( sPix ),
                                  0, 0, 
                                  JSPROP_ENUMERATE
                                  |JSPROP_PERMANENT
                                  |JSPROP_READONLY );
               JS_DefineProperty( cx, rImage, "isLoaded", 
                                  BOOLEAN_TO_JSVAL( true ),
                                  0, 0, 
                                  JSPROP_ENUMERATE
                                  |JSPROP_PERMANENT
                                  |JSPROP_READONLY );
               *rval = OBJECT_TO_JSVAL( rImage );
            }
            else
            {
               JS_ReportError( cx, "Error allocating pixStr" );
               JS_free( cx, pixels );
            }
         }
         else
            JS_ReportError( cx, "Error allocating pixBuf" );
      }
      else
         JS_ReportError( cx, "Error allocating image rect" );
      
      delete [] (unsigned char *)rgb ;
   }
   else
      JS_ReportError( cx, "must be capturing to grab" );
   return JS_TRUE ;
}

static JSBool
jsStop( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   cameraThreadParams_t *const params = (cameraThreadParams_t *)JS_GetPrivate( cx, obj );
   if( params )
   {
      close( params->fd_ );
      void *exitStat ;
      pthread_join( params->tHandle_, &exitStat );
      delete params ;
      JS_SetPrivate( cx, obj, 0 );
   }

   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static void *displayThread( void *arg )
{
   cameraThreadParams_t *const params = (cameraThreadParams_t *)arg ;

   struct video_picture    vidpic ; 
   ioctl( params->fd_, VIDIOCGPICT, &vidpic);
   vidpic.palette = VIDEO_PALETTE_RGB24 ;
   ioctl( params->fd_, VIDIOCSPICT, &vidpic);
   
   struct video_capability vidcap ; 
   ioctl( params->fd_, VIDIOCGCAP, &vidcap);

   struct video_window     vidwin ;
   ioctl( params->fd_, VIDIOCGWIN, &vidwin);
   
   // clamp width to camera x-resolution
   if( (int)params->w_ < vidcap.maxwidth )
   {
      vidwin.x   = ( vidcap.maxwidth - params->w_ ) / 2 ;
   } // center horizontally
   else
   {
      vidwin.x   = 0 ;
      params->w_ = vidcap.maxwidth ;
   }

   // clamp height to camera y-resolution
   if( (int)params->h_ < vidcap.maxheight )
   {
      vidwin.y   = ( vidcap.maxheight - params->h_ ) / 2 ;
   } // center vertically
   else
   {
      vidwin.y   = 0 ;
      params->h_ = vidcap.maxheight ;
   }

   fbDevice_t &fb = getFB();
   if( params->x_ + params->w_ > fb.getWidth() )
   {
      if( params->x_ > fb.getWidth() )
         params->x_ = 0 ;
      params->w_ = fb.getWidth() - params->x_ ;
   } // too wide for screen... clamp

   if( params->y_ + params->h_ > fb.getHeight() )
   {
      if( params->y_ > fb.getHeight() )
         params->y_ = 0 ;
      params->h_ = fb.getHeight() - params->y_ ;
   } // too tall for screen... clamp

   vidwin.width  = params->w_ ;
   vidwin.height = params->h_ ;
   ioctl( params->fd_, VIDIOCSWIN, &vidwin);

   unsigned const bufSize = params->h_ *params->w_ * 3 ;

   struct video_mbuf vidbuf;
   if( 0 <= ioctl( params->fd_, VIDIOCGMBUF, &vidbuf) )
   { 
      unsigned char *const buf = (unsigned char *)mmap( NULL, vidbuf.size, PROT_READ, MAP_SHARED, params->fd_, 0);
      if( buf )
      {
         unsigned frameCnt = 0 ;
         while( 1 )
         {
            int numframe = 0;
	    if( 0 <= ioctl( params->fd_, VIDIOCSYNC, &numframe) )
            {
               frameCnt++ ;
               if( params->wantImage_ )
               {
                  pthread_mutex_lock( &params->mutex_ );
                  params->imgBuf_ = new unsigned char [ bufSize ];
                  memcpy( params->imgBuf_, buf, bufSize );
                  params->wantImage_ = false ;
                  pthread_cond_signal( &params->condition_ );
                  pthread_mutex_unlock( &params->mutex_ );
               }
      
               unsigned offs = 0 ;
               for( unsigned y = 0 ; y < params->h_ ; y++ )
               {
                  for( unsigned x = 0 ; x < params->w_ ; x++, offs += 3 )
                  {
                     fb.setPixel( x+params->x_, y+params->y_, fb.get16( buf[offs+2], buf[offs+1], buf[offs] ) );
                  }
               }
            }
            else
            {
               printf("VIDIOCSYNC %m, frame:%d\n",frameCnt);
               break;
	    }
         }

         munmap( buf, vidbuf.size );
      }
      else
         perror( "mmap" );
   }
   else
      perror( "VIDIOCGMBUF" );
      
   return 0 ;
}

static JSBool
jsDisplay( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   cameraThreadParams_t *params = (cameraThreadParams_t *)JS_GetPrivate( cx, obj );
   if( params )
   {
      jsval tmp ;
      jsStop( cx, obj, 0, 0, &tmp );
   }
   
   if( ( 4 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] )
       &&
       JSVAL_IS_INT( argv[3] ) )
   {
      jsval deviceNameVal ;
      JS_GetProperty( cx, obj, "device", &deviceNameVal );
      JSString *sDeviceName = JSVAL_TO_STRING( deviceNameVal );

      params = new cameraThreadParams_t( JSVAL_TO_INT( argv[0] ),
                                         JSVAL_TO_INT( argv[1] ),
                                         JSVAL_TO_INT( argv[2] ),
                                         JSVAL_TO_INT( argv[3] ) );
      char const * const deviceName = JS_GetStringBytes( sDeviceName );

      params->fd_ = open( deviceName, O_RDWR );
      if( 0 <= params->fd_ )
      {
         int const create = pthread_create( &params->tHandle_, 0, displayThread, params );
         if( 0 == create )
         {
            JS_SetPrivate( cx, obj, params );
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "%m:displayThread" );
      }
      else
         JS_ReportError( cx, "%s: %m", deviceName );

      if( JSVAL_FALSE == *rval )
         delete params ;
   }
   else
      JS_ReportError( cx, "Usage: Camera.display( x, y, w, h )" );

   return JS_TRUE ;
}

static void *captureThread( void *arg )
{
   cameraThreadParams_t *const params = (cameraThreadParams_t *)arg ;

   struct video_picture    vidpic ; 
   ioctl( params->fd_, VIDIOCGPICT, &vidpic);
   vidpic.palette = VIDEO_PALETTE_RGB24 ;
   ioctl( params->fd_, VIDIOCSPICT, &vidpic);
   
   struct video_capability vidcap ; 
   ioctl( params->fd_, VIDIOCGCAP, &vidcap);

   struct video_window     vidwin ;
   ioctl( params->fd_, VIDIOCGWIN, &vidwin);
   
   // clamp width to camera x-resolution
   if( (int)params->w_ < vidcap.maxwidth )
   {
      vidwin.x   = ( vidcap.maxwidth - params->w_ ) / 2 ;
   } // center horizontally
   else
   {
      vidwin.x   = 0 ;
      params->w_ = vidcap.maxwidth ;
   }

   // clamp height to camera y-resolution
   if( (int)params->h_ < vidcap.maxheight )
   {
      vidwin.y   = ( vidcap.maxheight - params->h_ ) / 2 ;
   } // center vertically
   else
   {
      vidwin.y   = 0 ;
      params->h_ = vidcap.maxheight ;
   }

   vidwin.width  = params->w_ ;
   vidwin.height = params->h_ ;
   ioctl( params->fd_, VIDIOCSWIN, &vidwin);

   unsigned const bufSize = params->h_ *params->w_ * 3 ;
   struct video_mbuf vidbuf;
   if( 0 <= ioctl( params->fd_, VIDIOCGMBUF, &vidbuf) )
   { 
      unsigned char *const buf = (unsigned char *)mmap( NULL, vidbuf.size, PROT_READ, MAP_SHARED, params->fd_, 0);
      if( buf )
      {
         unsigned frameCnt = 0 ;
         while( 1 )
         {
            int numframe = 0;
	    if( 0 <= ioctl( params->fd_, VIDIOCSYNC, &numframe) )
            {
               frameCnt++ ;
               if( params->wantImage_ )
               {
                  pthread_mutex_lock( &params->mutex_ );
                  params->imgBuf_ = new unsigned char [ bufSize ];
                  memcpy( params->imgBuf_, buf, bufSize );
                  params->wantImage_ = false ;
                  pthread_cond_signal( &params->condition_ );
                  pthread_mutex_unlock( &params->mutex_ );
               }
            }
            else
            {
               printf("VIDIOCSYNC %m, frame:%d\n",frameCnt);
               break;
	    }
         }

         munmap( buf, vidbuf.size );
      }
      else
         perror( "mmap" );
   }
   else
      perror( "VIDIOCGMBUF" );
      
   return 0 ;
}

static JSBool
jsCapture( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   cameraThreadParams_t *params = (cameraThreadParams_t *)JS_GetPrivate( cx, obj );
   if( params )
   {
      jsval tmp ;
      jsStop( cx, obj, 0, 0, &tmp );
   }
   
   if( ( 2 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] ) )
   {
      jsval deviceNameVal ;
      JS_GetProperty( cx, obj, "device", &deviceNameVal );
      JSString *sDeviceName = JSVAL_TO_STRING( deviceNameVal );

      params = new cameraThreadParams_t( 0, 0, 
                                         JSVAL_TO_INT( argv[0] ),
                                         JSVAL_TO_INT( argv[1] ) );
      char const * const deviceName = JS_GetStringBytes( sDeviceName );

      params->fd_ = open( deviceName, O_RDWR );
      if( 0 <= params->fd_ )
      {
         int const create = pthread_create( &params->tHandle_, 0, captureThread, params );
         if( 0 == create )
         {
            JS_SetPrivate( cx, obj, params );
            *rval = JSVAL_TRUE ;
         }
         else
            JS_ReportError( cx, "%m:displayThread" );
      }
      else
         JS_ReportError( cx, "%s: %m", deviceName );

      if( JSVAL_FALSE == *rval )
         delete params ;
   }
   else
      JS_ReportError( cx, "Usage: Camera.capturer( w, h )" );

   return JS_TRUE ;
}

static JSBool
jsSetContrast( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSBool
jsSetBrightness( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_TRUE ;
   return JS_TRUE ;
}

static JSFunctionSpec camera_methods[] = {
   { "display",         jsDisplay,        0,0,0 },
   { "capture",         jsCapture,        0,0,0 },
   { "grab",            jsGrab,           0,0,0 },
   { "stop",            jsStop,           0,0,0 },
   { "setContrast",     jsSetContrast,    0,0,0 },
   { "setBrightness",   jsSetBrightness,  0,0,0 },
   { 0 }
};

static JSBool jsCamera( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 1 == argc ) && JSVAL_IS_STRING( argv[0] ) )
   {
      JSString *sDevice = JSVAL_TO_STRING( argv[0] );
      JSObject *camera = JS_NewObject( cx, &jsCameraClass_, NULL, NULL );
   
      if( camera )
      {
         JS_DefineProperty( cx, camera, "device",
                            STRING_TO_JSVAL( sDevice ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
         char const *const devicename = JS_GetStringBytes( sDevice );
         int const fd = open(devicename, O_RDWR);
         if( fd >= 0 )
         {
            struct video_capability vidcap ; 
            struct video_window     vidwin ;
            struct video_picture    vidpic ; 
       
            ioctl( fd, VIDIOCGCAP, &vidcap);
            ioctl( fd, VIDIOCGWIN, &vidwin);
            ioctl( fd, VIDIOCGPICT, &vidpic);
            vidpic.palette = VIDEO_PALETTE_RGB24 ;
            ioctl( fd, VIDIOCSPICT, &vidpic);
            close( fd );
            JS_DefineProperty( cx, camera, "name",
                               STRING_TO_JSVAL( JS_NewStringCopyZ( cx, vidcap.name ) ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "type",
                               INT_TO_JSVAL( vidcap.type ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "minWidth",
                               INT_TO_JSVAL( vidcap.minwidth ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "maxWidth",
                               INT_TO_JSVAL( vidcap.maxwidth ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "minHeight",
                               INT_TO_JSVAL( vidcap.minheight ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "maxHeight",
                               INT_TO_JSVAL( vidcap.maxheight ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "displayX",
                               INT_TO_JSVAL( vidwin.x ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "displayY",
                               INT_TO_JSVAL( vidwin.y ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "width",
                               INT_TO_JSVAL( vidwin.width ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "height",
                               INT_TO_JSVAL( vidwin.height ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "depth",
                               INT_TO_JSVAL( vidpic.depth ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
            JS_DefineProperty( cx, camera, "color",
                               BOOLEAN_TO_JSVAL( 0 == ( vidcap.type & VID_TYPE_MONOCHROME ) ),
                               0, 0, 
                               JSPROP_ENUMERATE
                               |JSPROP_PERMANENT
                               |JSPROP_READONLY );
         }
         else
            JS_ReportError( cx, "Invalid camera device %s", devicename );

         JS_SetPrivate( cx, camera, 0 );

         *rval = OBJECT_TO_JSVAL(camera);
      }
      else
         JS_ReportError( cx, "Allocating camera device" );

   }
   else
      JS_ReportError( cx, "Usage: Camera( '/dev/video0' )" );

   return JS_TRUE ;
}

bool initJSCamera( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsCameraClass_,
                                  jsCamera, 1,
                                  cameraProperties_, 
                                  camera_methods,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   return false ;
}

void shutdownCamera()
{
}

