/*
 * Module jsAlphaMap.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: jsAlphaMap.cpp,v $
 * Revision 1.4  2003-05-26 22:07:20  ericn
 * -added method rotate()
 *
 * Revision 1.3  2003/01/12 03:04:41  ericn
 * -fixed colormap
 *
 * Revision 1.2  2002/12/15 20:01:57  ericn
 * -modified to use JS_NewObject instead of js_NewObject
 *
 * Revision 1.1  2002/11/02 18:39:42  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "jsAlphaMap.h"
#include "js/jscntxt.h"
#include "fbDev.h"

static JSBool
jsAlphaMapDraw( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 3 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] ) )
   {
      fbDevice_t &fb = getFB();

      int const specX = JSVAL_TO_INT( argv[0] );
      int const specY = JSVAL_TO_INT( argv[1] );
      unsigned const rgb   = JSVAL_TO_INT( argv[2] );

      jsval     vPixMap ;
      jsval     vWidth ;
      jsval     vHeight ;
      JSString *sPixMap ;

      if( JS_GetProperty( cx, obj, "pixBuf", &vPixMap )
          &&
          JSVAL_IS_STRING( vPixMap )
          &&
          ( 0 != ( sPixMap = JSVAL_TO_STRING( vPixMap ) ) )
          &&
          JS_GetProperty( cx, obj, "width", &vWidth )
          &&
          JSVAL_IS_INT( vWidth )
          &&
          JS_GetProperty( cx, obj, "height", &vHeight )
          &&
          JSVAL_IS_INT( vHeight ) )
      {
         unsigned const bmWidth    = JSVAL_TO_INT( vWidth );
         unsigned const bmHeight   = JSVAL_TO_INT( vHeight );
/*
         printf( "string gets drawn here\n"
                 "x        %u\n"
                 "y        %u\n"
                 "rgb      0x%06x\n"
                 "w        %u\n"
                 "h        %u\n", specX, specY, rgb, bmWidth, bmHeight );
*/                 
         //
         // user specifies position of baseline, adjust to top of bitmap
         //
         unsigned bmStartY = 0 ;
         int screenStartY = specY ;
         if( 0 > screenStartY )
         {
            bmStartY = ( 0-screenStartY );
            screenStartY = 0 ;
         } // starts off the screen, walk down

         unsigned bmStartX = 0 ;
         int screenStartX = specX ;
         if( 0 > screenStartX )
         {
            bmStartX = ( 0 - screenStartX );
            screenStartX  = 0 ;
         } // starts off the screen, walk right

         if( ( bmWidth > bmStartX ) && ( screenStartX <= fb.getWidth() ) )
         {
            //
            // point row at start row/col
            //
            unsigned char const *inRow = (unsigned char const *)JS_GetStringBytes( sPixMap )
                                       + (bmWidth*bmStartY) + bmStartX ;
            //
            // draw every scanline
            //
            unsigned const red   = (unsigned char)( rgb >> 16 );
            unsigned const green = (unsigned char)( rgb >> 8 );
            unsigned const blue  = (unsigned char)rgb ;

            unsigned screenY = screenStartY ;
            unsigned short const fullColor = fb.get16( red, green, blue );
            
            for( unsigned bmY = bmStartY ; ( bmY < bmHeight ) && ( screenY < fb.getHeight() ) ; bmY++, screenY++, inRow += bmWidth )
            {
               unsigned char const *nextIn = inRow ;
               unsigned screenX = screenStartX ;
               for( unsigned bmx = bmStartX ; ( bmx < bmWidth ) && ( screenX < fb.getWidth() ) ; bmx++, screenX++ )
               {
                  unsigned char const alpha = *nextIn++ ;
                  if( 0 == alpha )
                  {
                  } // nothing
                  else if( 255 == alpha )
                  {
                     fb.getPixel( screenX, screenY ) = fullColor ;
                  } // full color
                  else
                  {
                     unsigned short const screen16 = fb.getPixel( screenX, screenY );
                     unsigned const screenRed   = fb.getRed( screen16 );
                     unsigned const screenGreen = fb.getGreen( screen16 );
                     unsigned const screenBlue  = fb.getBlue( screen16 );
                     unsigned char const bg255ths = ~alpha ;
                     unsigned char mixRed = ( ( screenRed * bg255ths ) + ( alpha * red ) ) / 256 ;
                     unsigned char mixGreen = ( ( screenGreen * bg255ths ) + ( alpha * green ) ) / 256 ;
                     unsigned char mixBlue = ( ( screenBlue * bg255ths ) + ( alpha * blue ) ) / 256 ;
                     unsigned short outColor = fb.get16( mixRed, mixGreen, mixBlue );
                     fb.getPixel( screenX, screenY ) = outColor ;
                  }
//                  printf( "%02x", alpha );
               } // draw entire row
//               printf( "\n" );
            }
/*
            printf( "\n\n" );
            unsigned char const *byteMap = (unsigned char const *)JS_GetStringBytes( sPixMap );
            for( unsigned y = 0 ; y < bmHeight ; y++ )
            {
               for( unsigned x = 0 ; x < bmWidth ; x++ )
                  printf( "%02x", byteMap[ y * bmWidth + x ] );
               printf( "\n" );
            }
*/            
         } // room for something
      }
      else
         JS_ReportError( cx, "Error retrieving alphaMap fields" );
   }
   else
      JS_ReportError( cx, "Usage: alphaMap.draw( x, y, RGB );" );

   return JS_TRUE ;

}

static JSBool
jsAlphaMapRotate( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   jsval widthVal, heightVal, dataVal ;
   if( JS_GetProperty( cx, obj, "width", &widthVal )
       &&
       JS_GetProperty( cx, obj, "height", &heightVal )
       &&
       JS_GetProperty( cx, obj, "pixBuf", &dataVal )
       &&
       JSVAL_IS_STRING( dataVal ) )
   {
      int const width  = JSVAL_TO_INT( widthVal );
      int const height = JSVAL_TO_INT( heightVal );
      JSString *pixStr = JSVAL_TO_STRING( dataVal );
      unsigned char const *const pixMap = (unsigned char *)JS_GetStringBytes( pixStr );
      if( JS_GetStringLength( pixStr ) == width * height * sizeof( pixMap[0] ) )
      {
         JSObject *returnObj = JS_NewObject( cx, &jsAlphaMapClass_, 0, 0 );
         if( returnObj )
         {
            *rval = OBJECT_TO_JSVAL( returnObj ); // root
            unsigned const pixBytes = width*height ;
            void *const pixMem = JS_malloc( cx, pixBytes );
            JSString *sAlphaMap = JS_NewString( cx, (char *)pixMem, pixBytes );
            if( sAlphaMap )
            {
               JS_DefineProperty( cx, returnObj, "pixBuf", STRING_TO_JSVAL( sAlphaMap ), 0, 0, JSPROP_ENUMERATE );

               enum rotate_e {
                  rotate90_e,
                  rotate180_e,
                  rotate270_e
               };
         
               int const degrees = JSVAL_TO_INT( argv[0] );
               rotate_e  rotation ;
               if( 90 == degrees )
                  rotation = rotate90_e ;
               else if( 180 == degrees )
                  rotation = rotate180_e ;
               else if( 270 == degrees )
                  rotation = rotate270_e ;
               else
               {
                  JS_ReportError( cx, "Only 90, 180, 270 degree rotations supported\n" );
                  return JS_TRUE ;
               }
         
               //
               // point row at start row/col
               //
               unsigned char *const output = (unsigned char *)pixMem ;
      
               switch( rotation )
               {
                  case rotate90_e :
                     {
                        for( unsigned y = 0 ; y < height ; y++ )
                        {
                           for( unsigned x = 0 ; x < width ; x++ )
                           {
                              output[y+x*height] = pixMap[x+(height-y-1)*width];
                           }
                        }
      
                        break;
                     }
                  case rotate180_e :
                     {
                        for( unsigned y = 0 ; y < height ; y++ )
                        {
                           for( unsigned x = 0 ; x < width ; x++ )
                           {
                              output[y*width+x] = pixMap[((height-y-1)*width)+width-x-1];
                           }
                        }
                        break;
                     }
                  case rotate270_e :
                     {
                        for( unsigned y = 0 ; y < height ; y++ )
                        {
                           for( unsigned x = 0 ; x < width ; x++ )
                           {
                              output[y+x*height] = pixMap[width-x-1+y*width];
                           }
                        }

                        break;
                     }
               }
      
               if( ( rotate90_e == rotation ) 
                   ||
                   ( rotate270_e == rotation ) )
               {
                  JS_DefineProperty( cx, returnObj, "width", heightVal, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
                  JS_DefineProperty( cx, returnObj, "height", widthVal, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               } // swap width and height
               else
               {
                  JS_DefineProperty( cx, returnObj, "width", widthVal, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
                  JS_DefineProperty( cx, returnObj, "height", heightVal, 0, 0, JSPROP_READONLY|JSPROP_ENUMERATE );
               }
            }
            else
               JS_ReportError( cx, "Error building alpha map string" );
         }
         else
            JS_ReportError( cx, "allocating array" );
      }
      else
         JS_ReportError( cx, "Invalid width or height" );
   }
   else
      JS_ReportError( cx, "Invalid image" );

   return JS_TRUE ;
}

static JSFunctionSpec alphaMapMethods_[] = {
    {"draw",         jsAlphaMapDraw,           3 },
    {"rotate",       jsAlphaMapRotate,         3 },
    {0}
};

JSClass jsAlphaMapClass_ = {
  "alphaMap",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,     JS_PropertyStub,
   JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,      JS_FinalizeStub,
   JSCLASS_NO_OPTIONAL_MEMBERS
};

JSPropertySpec alphaMapProperties_[ALPHAMAP_NUMPROPERTIES+1] = {
  {"width",         ALPHAMAP_WIDTH,     JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"height",        ALPHAMAP_HEIGHT,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {"pixBuf",        ALPHAMAP_PIXBUF,    JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT },
  {0,0,0}
};

static JSBool alphaMap( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   if( ( 1 == argc ) 
       && 
       ( 0 != (cx->fp->flags & JSFRAME_CONSTRUCTING) ) 
       &&
       JSVAL_IS_OBJECT( argv[0] ) )
   {
      JSObject *thisObj = JS_NewObject( cx, &jsAlphaMapClass_, NULL, NULL );

      if( thisObj )
      {
         *rval = OBJECT_TO_JSVAL( thisObj ); // root
      }
      else
         JS_ReportError( cx, "Error allocating curlFile" );
   }
   else
      JS_ReportError( cx, "Usage : new alphaMap( { url:\"something\" } );" );
      
   return JS_TRUE ;

}

bool initJSAlphaMap( JSContext *cx, JSObject *glob )
{
   JSObject *rval = JS_InitClass( cx, glob, NULL, &jsAlphaMapClass_,
                                  alphaMap, 1,
                                  alphaMapProperties_, 
                                  alphaMapMethods_,
                                  0, 0 );
   if( rval )
   {
      return true ;
   }
   
   return false ;
}

