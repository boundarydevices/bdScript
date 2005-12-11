/*
 * Module jsCairo.cpp
 *
 * This module defines the methods of the cairo class.
 *
 *
 * Change History : 
 *
 * $Log: jsCairo.cpp,v $
 * Revision 1.1  2005-12-11 16:01:57  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2005
 */

#include "jsCairo.h"
#include "image.h"

static JSObject *cairoProto = NULL ;
static JSObject *cairoSurfProto = NULL ;
static JSObject *cairoCtxProto = NULL ;
static JSObject *cairoPatProto = NULL ;

static JSBool
jsSurfaceStatus( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_surface_t *const surf = surfObject( cx, obj );
   if( surf )
   {
      *rval = INT_TO_JSVAL( cairo_surface_status(surf) );
   }
   return JS_TRUE ;
}

static JSBool
jsSurfaceWidth( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_surface_t *const surf = surfObject( cx, obj );
   if( surf )
   {
      *rval = INT_TO_JSVAL( cairo_image_surface_get_width(surf) );
   }
   return JS_TRUE ;
}

static JSBool
jsSurfaceHeight( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_surface_t *const surf = surfObject( cx, obj );
   if( surf )
   {
      *rval = INT_TO_JSVAL( cairo_image_surface_get_height(surf) );
   }
   return JS_TRUE ;
}

static JSBool
jsWritePNG( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_surface_t *const surf = surfObject( cx, obj );
   if( surf )
   {
      JSString   *sFileName ;
      char const *fileName ;
      if( ( 1 == argc )
          &&
          JSVAL_IS_STRING( argv[0] )
          &&
          ( 0 != (sFileName = JSVAL_TO_STRING(argv[0]) ) ) 
          &&
          ( 0 != (fileName = JS_GetStringBytes(sFileName) ) ) )
      {
         cairo_surface_flush(surf);
         printf( "write to %s here\n", fileName );
         cairo_status_t const status = cairo_surface_write_to_png(surf,fileName);
         printf( "status == %d\n", status );
         *rval = INT_TO_JSVAL( status );
      }
   }
   return JS_TRUE ;
}

static JSFunctionSpec surf_methods_[] = {
   { "status",       jsSurfaceStatus,           0,0,0 },
   { "width",        jsSurfaceWidth,            0,0,0 },
   { "height",       jsSurfaceHeight,           0,0,0 },
   { "writePNG",     jsWritePNG,                0,0,0 },
   { 0 }
};

static void jsSurfaceFinalize(JSContext *cx, JSObject *obj);

JSClass jsCairoSurfaceClass_ = {
  "cairo_surface",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsSurfaceFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec surf_properties_[] = {
  {0,0,0}
};

static void jsSurfaceFinalize(JSContext *cx, JSObject *obj)
{
   if( obj )
   {
      cairo_surface_t *const surf = surfObject( cx, obj );
      if( surf )
      {
fprintf( stderr, "---> finalize surf %p\n", surf );
         JS_SetPrivate( cx, obj, 0 );
         cairo_surface_finish(surf);
         cairo_surface_destroy(surf);
      } // have button data
   }
}

unsigned getCairoStride( 
   cairo_format_t format,
   unsigned width )
{
   switch( format )
   {
      case CAIRO_FORMAT_ARGB32: return 4*width ;
      case CAIRO_FORMAT_RGB24:  return 3*width ;
      case CAIRO_FORMAT_A8:     return width ;
      case CAIRO_FORMAT_A1:     return ((width+7)/8)*8;
   }
   
   return 0 ;
   
}

cairo_user_data_key_t const cairoPixelKey_ = { 0 };

static void freeSurfData( void *data )
{
   delete [] (unsigned char *)data ;
   printf( "free surface data %p\n", data );
}

static JSBool jsCairoSurface( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( (3 == argc)
       &&
       JSVAL_IS_INT(argv[0])
       &&
       JSVAL_IS_INT(argv[1])
       &&
       JSVAL_IS_INT(argv[2]) )
   {
      JSObject *robj = JS_NewObject( cx, &jsCairoSurfaceClass_, NULL, NULL );
   
      if( robj )
      {
         *rval = OBJECT_TO_JSVAL(robj); // root
         cairo_format_t format = (cairo_format_t)JSVAL_TO_INT(argv[0]);
         unsigned const width = JSVAL_TO_INT(argv[1]);
         unsigned const height = JSVAL_TO_INT(argv[2]);
         unsigned const stride = getCairoStride(format,width);
         unsigned const databytes = height*stride ;
         unsigned char *const data = new unsigned char [databytes];
         cairo_surface_t* surf = cairo_image_surface_create_for_data(
            data, format, width, height, stride );
         printf( "surface == %p, data == %p\n", surf, data );

         cairo_surface_set_user_data( surf, &cairoPixelKey_, data, freeSurfData );
         JS_SetPrivate( cx, robj, surf );
      }
   }
   else
      JS_ReportError( cx, "Usage: cairo_surface( format, w, h )" );
   
   return JS_TRUE;
}

static JSBool
jsRectangle( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_t *cr = ctxObject( cx, obj );
   if( cr && ( 4 == argc ) )
   {
      double dparams[4];
      for( unsigned i = 0 ; i < 4 ; i++ )
      {
         if( !JS_ValueToNumber( cx, argv[i], dparams+i ) )
         {
            JS_ReportError( cx, 
                            "Invalid parameter: values in [0..1]\n" );
            return JS_TRUE ;
         }
      }
      
      cairo_rectangle(cr,dparams[0],dparams[1],dparams[2],dparams[3]);
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Usage: context.rectangle(x,y,w,h)\n"
                      "See cairo_rectangle()" );
   return JS_TRUE ;
}

static JSBool
jsArc( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_t *cr = ctxObject( cx, obj );
   if( cr && ( 5 == argc ) )
   {
      double dparams[5];
      for( unsigned i = 0 ; i < 5 ; i++ )
      {
         if( !JS_ValueToNumber( cx, argv[i], dparams+i ) )
         {
            JS_ReportError( cx, 
                            "Invalid parameter: values in [0..1]\n" );
            return JS_TRUE ;
         }
      }

      cairo_arc(cr,dparams[0],dparams[1],dparams[2],dparams[3],dparams[4]);
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Usage: context.arc(xc,yc,radius,angle1,angle2)\n"
                      "See cairo_arc()" );
   return JS_TRUE ;
}

static JSBool
jsSetSource( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_t  *cr = ctxObject( cx, obj );
   JSObject *patObj ;
   cairo_pattern_t *pat ;
   
   if( ( 0 != cr )
       && 
       ( 1 == argc ) 
       &&
       ( 0 != (patObj = JSVAL_TO_OBJECT(argv[0]) ) ) 
       &&
       ( 0 != (pat = patObject(cx,patObj) ) ) )
   {
      cairo_set_source(cr,pat);
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Usage: context.setSource(pattern)\n"
                      "See cairo_set_source()" );
   return JS_TRUE ;
}

static JSBool
jsFill( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_t  *cr = ctxObject( cx, obj );
   
   if( 0 != cr )
   {
      cairo_fill(cr);
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Usage: context.fill()\n"
                      "See cairo_fill()" );
   return JS_TRUE ;
}

static JSBool
jsScale( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_t  *cr = ctxObject( cx, obj );
   double w ;
   double h ;

   if( ( 0 != cr )
       &&
       ( 2 == argc ) 
       &&
       JS_ValueToNumber( cx, argv[0], &w )
       &&
       JS_ValueToNumber( cx, argv[1], &h ) )
   {
      cairo_scale( cr, w, h );
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Usage: context.scale(w,h)\n"
                      "See cairo_scale()" );
   return JS_TRUE ;
}

static void jsContextFinalize(JSContext *cx, JSObject *obj);

JSClass jsCairoContextClass_ = {
  "cairo_context",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsContextFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec ctx_properties_[] = {
  {0,0,0}
};

static JSFunctionSpec ctx_methods_[] = {
   { "rectangle",        jsRectangle,    0,0,0 },
   { "arc",              jsArc,          0,0,0 },
   { "setSource",        jsSetSource,    0,0,0 },
   { "fill",             jsFill,         0,0,0 },
   { "scale",            jsScale,        0,0,0 },
   { 0 }
};

static void jsContextFinalize(JSContext *cx, JSObject *obj)
{
   if( obj )
   {
      cairo_t *const ctx = ctxObject( cx, obj );
      if( ctx )
      {
fprintf( stderr, "---> finalize ctx %p\n", ctx );
         JS_SetPrivate( cx, obj, 0 );
         cairo_destroy(ctx);
      } // have button data
   }
}

static JSBool
jsCairoContext( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   JSObject *surfObj = 0 ;
   cairo_surface_t *surf = 0 ;
   if( ( 1 == argc )
       &&
       JSVAL_IS_OBJECT( argv[0] ) 
       &&
       ( 0 != (surfObj = JSVAL_TO_OBJECT( argv[0] ) ) )
       &&
       JS_InstanceOf( cx, surfObj, &jsCairoSurfaceClass_, NULL ) 
       &&
       ( 0 != (surf = surfObject( cx, surfObj ) ) ) )
   {
      JSObject *ctxObj = JS_NewObject( cx, &jsCairoContextClass_, NULL, NULL );
      if( ctxObj )
      {
         *rval = OBJECT_TO_JSVAL(ctxObj);
         cairo_t *ctx = cairo_create( surf );
         JS_SetPrivate( cx, ctxObj, ctx );
      }
   }
   else
   {
      JS_ReportError( cx, "Usage: cairo.context(surface)" );
   }
   return JS_TRUE ;
}

static void jsPatternFinalize(JSContext *cx, JSObject *obj);

static JSBool
jsAddColorStopRGB( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_pattern_t *pat = patObject( cx, obj );
   if( ( 4 == argc )
       &&
       ( 0 != pat ) )
   {
      double dparams[4];
      for( unsigned i = 0 ; i < 4 ; i++ )
      {
         if( !JS_ValueToNumber( cx, argv[i], dparams+i ) )
         {
            JS_ReportError( cx, 
                            "Invalid parameter: values in [0..1]\n" );
            return JS_TRUE ;
         }
      }
      cairo_pattern_add_color_stop_rgb( 
         pat, 
         dparams[0],
         dparams[1],
         dparams[2],
         dparams[3] );
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Usage: pattern.colorStopRGB(offset,red,green,blue)\n"
                      "See cairo_pattern_add_color_stop_rgb" );

   return JS_TRUE  ;
}

static JSBool
jsAddColorStopRGBA( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   cairo_pattern_t *pat = patObject( cx, obj );
   if( ( 5 == argc )
       &&
       ( 0 != pat ) )
   {
      double dparams[5];
      for( unsigned i = 0 ; i < 5 ; i++ )
      {
         if( !JS_ValueToNumber( cx, argv[i], dparams+i ) )
         {
            JS_ReportError( cx, 
                            "Invalid parameter: values in [0..1]\n" );
            return JS_TRUE ;
         }
      }
      cairo_pattern_add_color_stop_rgba( 
         pat, 
         dparams[0],
         dparams[1],
         dparams[2],
         dparams[3],
         dparams[4] );
      *rval = JSVAL_TRUE ;
   }
   else
      JS_ReportError( cx, "Usage: pattern.colorStopRGBA(offset,red,green,blue,alpha)\n"
                      "See cairo_pattern_add_color_stop_rgba" );
   return JS_TRUE  ;
}

JSClass jsCairoPatternClass_ = {
  "cairo_pattern",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      jsPatternFinalize,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSPropertySpec pat_properties_[] = {
  {0,0,0}
};

static JSFunctionSpec pat_methods_[] = {
   { "colorStopRGB",         jsAddColorStopRGB,           0,0,0 },
   { "colorStopRGBA",        jsAddColorStopRGBA,          0,0,0 },
   { 0 }
};

static void jsPatternFinalize(JSContext *cx, JSObject *obj)
{
   if( obj )
   {
      cairo_pattern_t *const pat = patObject( cx, obj );
      if( pat )
      {
fprintf( stderr, "---> finalize pat %p\n", pat );
         JS_SetPrivate( cx, obj, 0 );
         cairo_pattern_destroy(pat);
      } // have data
   }
}

static JSBool
jsLinearGradient( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( 4 == argc )
   {
      double dparams[4];
      for( unsigned i = 0 ; i < 4 ; i++ )
      {
         if( !JS_ValueToNumber( cx, argv[i], dparams+i ) )
         {
            JS_ReportError( cx, 
                            "Invalid parameter: values in [0..1]\n" );
            return JS_TRUE ;
         }
      }
      
      JSObject *patObj = JS_NewObject( cx, &jsCairoPatternClass_, NULL, NULL );
      if( patObj )
      {
         *rval = OBJECT_TO_JSVAL(patObj);
         cairo_pattern_t *pat = cairo_pattern_create_linear(
            dparams[0],
            dparams[1],
            dparams[2],
            dparams[3]
         );
         JS_SetPrivate( cx, patObj, pat );
      }
   }
   else
   {
      JS_ReportError( cx, 
                      "Usage: cairo.linearGradient(x0,y0,x1,y1)\n"
                      "(see cairo_pattern_create_linear())" );
   }
   return JS_TRUE ;
}

static JSBool
jsRadialGradient( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   
   if( 6 == argc )
   {
      double dparams[6];
      for( unsigned i = 0 ; i < 6 ; i++ )
      {
         if( !JS_ValueToNumber( cx, argv[i], dparams+i ) )
         {
            JS_ReportError( cx, 
                            "Invalid parameter: values in [0..1]\n" );
            return JS_TRUE ;
         }
      }
      
      JSObject *patObj = JS_NewObject( cx, &jsCairoPatternClass_, NULL, NULL );
      if( patObj )
      {
         *rval = OBJECT_TO_JSVAL(patObj);
         cairo_pattern_t *pat = cairo_pattern_create_radial(
            dparams[0],
            dparams[1],
            dparams[2],
            dparams[3],
            dparams[4],
            dparams[5]
         );
         JS_SetPrivate( cx, patObj, pat );
      }
   }
   else
   {
      JS_ReportError( cx, 
                      "Usage: cairo.radialGradient(cx0,cy0,radius0,cx1,cy1,radius1)\n"
                      "(see cairo_pattern_create_radial())" );
   }
   return JS_TRUE ;
}



JSClass jsCairoClass_ = {
  "cairo",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec methods_[] = {
   { "context",         jsCairoContext,           0,0,0 },
   { "linearGradient",  jsLinearGradient,         0,0,0 },
   { "radialGradient",  jsRadialGradient,         0,0,0 },
   { "surface",         jsCairoSurface,           0,0,0, },
   { 0 }
};

static JSPropertySpec properties_[] = {
  {0,0,0}
};

#define CAIRO_ENUM(__v) __v,
static int const cairoEnums_[] = {
   #include "cairoEnum.h"
};
#undef CAIRO_ENUM

#define CAIRO_ENUM(__v) #__v,
static char const *const cairoEnumNames_[] = {
   #include "cairoEnum.h"
};
#undef CAIRO_ENUM

static unsigned const numEnums = (sizeof( cairoEnums_ )/sizeof(cairoEnums_[0]));

//
// constructor for the screen object
//
static JSBool jsCairo( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   obj = JS_NewObject( cx, &jsCairoClass_, NULL, NULL );

   if( obj )
   {
      *rval = OBJECT_TO_JSVAL(obj); // root
   }
   else
      *rval = JSVAL_FALSE ;
   
   return JS_TRUE;
}

bool initJSCairo( JSContext *cx, JSObject *glob )
{
   cairoProto = JS_InitClass( 
      cx, glob, NULL, &jsCairoClass_,
      jsCairo, 1,
      properties_, 
      methods_,
      0, 0 
   );

   if( cairoProto )
   {
      JS_AddRoot( cx, &cairoProto );
      cairoSurfProto = JS_InitClass( 
         cx, glob, NULL, &jsCairoSurfaceClass_,
         jsCairoSurface, 1,
         surf_properties_, 
         surf_methods_,
         0, 0 
      );
      
      if( cairoSurfProto )
      {
         JS_AddRoot( cx, &cairoSurfProto );
         cairoCtxProto = JS_InitClass( 
            cx, glob, NULL, &jsCairoContextClass_,
            jsCairoContext, 1,
            ctx_properties_, 
            ctx_methods_,
            0, 0 
         );
         
         if( cairoCtxProto )
         {
            JS_AddRoot( cx, &cairoCtxProto );
            cairoPatProto = JS_InitClass( 
               cx, glob, NULL, &jsCairoPatternClass_,
               jsCairoContext, 1,
               pat_properties_, 
               pat_methods_,
               0, 0 
            );
            
            if( cairoPatProto )
            {
               JS_AddRoot( cx, &cairoPatProto );
               JSObject *cairo = JS_NewObject( cx, &jsCairoClass_, cairoProto, glob );
               if( cairo )
               {
                  JS_DefineProperty( cx, glob, "cairo", 
                                     OBJECT_TO_JSVAL( cairo ),
                                     0, 0, 
                                     JSPROP_ENUMERATE
                                     |JSPROP_PERMANENT
                                     |JSPROP_READONLY );
                  for( unsigned i = 0 ; i < numEnums ; i++ )
                  {
                     JS_DefineProperty( cx, cairo, cairoEnumNames_[i], 
                                        INT_TO_JSVAL( cairoEnums_[i] ),
                                        0, 0, 
                                        JSPROP_ENUMERATE
                                        |JSPROP_PERMANENT
                                        |JSPROP_READONLY );
                  }
               }
            }
         }
      }
   }
      
   return false ;
}

void shutdownJSCairo( void )
{
}

