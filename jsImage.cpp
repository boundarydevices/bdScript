/*
 * Module jsImage.cpp
 *
 * This module defines the JavaScript routines
 * which display images as described and declared 
 * in jsImage.h
 *
 *
 * Change History : 
 *
 * $Log: jsImage.cpp,v $
 * Revision 1.1  2002-10-13 16:32:25  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsImage.h"
#include "libpng12/png.h"

static JSBool
imagePNG(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   //
   // need at least data, plus
   //
   if( ( ( 1 == argc ) || ( 3 == argc ) )
       &&
       JSVAL_IS_STRING( argv[0] ) 
       &&
       ( ( 1 == argc ) 
         || 
         ( JSVAL_IS_INT( argv[1] )
           &&
           JSVAL_IS_INT( argv[2] ) ) ) )   // second and third parameters must be ints
   {
      JSString *str = JS_ValueToString( cx, argv[0] );
      if( str )
      {
         png_bytep pngData = (png_bytep)JS_GetStringBytes( str );
         unsigned const pngLength = JS_GetStringLength(str);
         printf( "have image string : length %u\n", pngLength );
         if( 8 < pngLength )
         {
            if( 0 == png_sig_cmp( pngData, 0, pngLength ) )
            {
               png_structp png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
               {
                  if( png_ptr )
                  {
                     png_infop info_ptr = png_create_info_struct(png_ptr);
                     if( info_ptr )
                     {
                        png_infop end_info = png_create_info_struct(png_ptr);
                        if( end_info )
                        {
                           printf( "allocated PNG structures\n" );
                           png_destroy_read_struct( &png_ptr, &info_ptr, &end_info );
                        }
                     }
                     else
                        png_destroy_read_struct( &png_ptr, 0, 0 );
                  }
               }
            }
            else
               printf( "Not a PNG file\n" );
         }
         else
         {
            printf( "PNG too short!\n" );
         }
      } // retrieved string

      *rval = JSVAL_FALSE ;
      return JS_TRUE ;

   } // need at least two params

   return JS_FALSE ;
}

static JSFunctionSpec image_functions[] = {
    {"imagePNG",         imagePNG,        1},
    {0}
};


bool initJSImage( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, image_functions);
}

