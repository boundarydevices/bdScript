/*
 * Module jsAlphaMap.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: jsAlphaMap.cpp,v $
 * Revision 1.10  2005-11-06 16:02:10  ericn
 * -KERNEL_FB, not CONFIG_BD2003
 *
 * Revision 1.9  2005/11/05 23:23:34  ericn
 * -fixed compiler warnings
 *
 * Revision 1.8  2004/06/27 14:50:31  ericn
 * -stub for game/suite controller
 *
 * Revision 1.7  2004/05/08 15:03:42  ericn
 * -removed debug statement
 *
 * Revision 1.6  2004/05/05 03:19:50  ericn
 * -added draw into image
 *
 * Revision 1.5  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.4  2003/05/26 22:07:20  ericn
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
#include "string.h"

static JSBool
jsCrop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval, bool isRight)
{
   *rval = JSVAL_FALSE;

   if((1 == argc)
      &&
      JSVAL_IS_INT(argv[0]))
   {
      jsval     vPixMap ;
      jsval     vWidth ;
      jsval     vHeight ;
      JSString *sPixMap ;

      if(JS_GetProperty(cx, obj, "pixBuf", &vPixMap)
         &&
         JSVAL_IS_STRING(vPixMap)
         &&
         (0 != (sPixMap = JSVAL_TO_STRING(vPixMap)))
         &&
         JS_GetProperty(cx, obj, "width", &vWidth)
         &&
         JSVAL_IS_INT(vWidth)
         &&
         JS_GetProperty(cx, obj, "height", &vHeight)
         &&
         JSVAL_IS_INT(vHeight))
      {
         unsigned       bmWidth    = JSVAL_TO_INT(vWidth);
         unsigned const bmHeight   = JSVAL_TO_INT(vHeight);
         
         /*
          * here we modify the in object's pixel data in memory
          * so we need not write the modified data back to the
          * object property.
          */
         unsigned char *inRow = (unsigned char *)JS_GetStringBytes(sPixMap);
         unsigned int displayWidth = JSVAL_TO_INT(argv[0]);
         if(displayWidth >= bmWidth) /* do nothing if display width is greater than image width */
            return JS_TRUE;
         /* crop only if display width is less than alphaMap width */
         unsigned int offset = isRight ? 0 : (bmWidth - displayWidth);
         if(bmWidth > displayWidth) { //modify pixMap
             unsigned char *newMapCurPos = inRow;
             unsigned char *oldMapCurPos = inRow + offset;
             for(unsigned int idx = 0; idx < bmHeight; idx++) {
                 memmove(newMapCurPos, oldMapCurPos, displayWidth);
                 newMapCurPos += displayWidth;
                 oldMapCurPos += bmWidth;
             }
             jsval tempWidth = INT_TO_JSVAL(displayWidth);
             JSBool ret = JS_SetProperty(cx, obj, "width", &tempWidth);
             if(ret == JS_FALSE)
                printf("Property is not set\n");
         }
      }
   }
   return JS_TRUE;
}

/*
 * Takes the display width of the alpha map and crops all the pixels
 * to the right of the display width
 */
static JSBool
jsCropRight(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   return jsCrop(cx, obj, argc, argv, rval, true);
}

/*
 * Takes the display width of the alpha map and crops all the pixels
 * to the left of the display width
 */
static JSBool
jsCropLeft(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
   return jsCrop(cx, obj, argc, argv, rval, false);
}

static JSBool
jsAlphaMapDraw( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   JSObject *imageObj = 0 ;
   if( ( 3 <= argc )
       &&
       ( 4 >= argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_INT( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] ) 
       &&
       ( ( 3 == argc )
         ||
         ( JSVAL_IS_OBJECT( argv[3] ) 
           &&
           ( 0 != ( imageObj = JSVAL_TO_OBJECT( argv[3] ) ) ) ) )
       )
   {
      fbDevice_t &fb = getFB();
      unsigned char *imageMem = 0 ;
      unsigned short  imageWidth = 0 ;
      unsigned short  imageHeight = 0 ;
      unsigned short  imageStride = 0;

      if( 3 == argc )
      {
#ifdef KERNEL_FB
         imageMem = (unsigned char *)fb.getRow(0);
         imageWidth = fb.getWidth();
         imageHeight = fb.getHeight();
         imageStride = fb.getStride();
#else
         JS_ReportError( cx, "draw alphaMap onto screen" );
         return JS_TRUE ;
#endif
      }
      else
      {
         jsval     vPixMap ;
         jsval     vWidth ;
         jsval     vHeight ;
         JSString *sPixMap ;
      
         if( JS_GetProperty( cx, imageObj, "pixBuf", &vPixMap )
             &&
             JSVAL_IS_STRING( vPixMap )
             &&
             ( 0 != ( sPixMap = JSVAL_TO_STRING( vPixMap ) ) )
             &&
             JS_GetProperty( cx, imageObj, "width", &vWidth )
             &&
             JSVAL_IS_INT( vWidth )
             &&
             JS_GetProperty( cx, imageObj, "height", &vHeight )
             &&
             JSVAL_IS_INT( vHeight ) )
         {
            unsigned const bmWidth    = JSVAL_TO_INT( vWidth );
            unsigned       bmHeight   = JSVAL_TO_INT( vHeight );
            if( JS_GetStringLength( sPixMap ) == bmWidth*bmHeight*2 )
            {
               imageMem = (unsigned char *)JS_GetStringBytes( sPixMap );
               imageWidth  = bmWidth ;
               imageHeight = bmHeight ;
               imageStride = imageWidth << 1;
            }
            else
               JS_ReportError( cx, "Error getting pixel data" );
         }
         else
            JS_ReportError( cx, "Invalid image" );
         
         if( !imageMem )
            return JS_TRUE ;
      }

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
         unsigned       bmWidth    = JSVAL_TO_INT( vWidth );
         unsigned const bmHeight   = JSVAL_TO_INT( vHeight );

         //printf( "string gets drawn here\n"
         //        "x        %u\n"
         //        "y        %u\n"
         //        "rgb      0x%06x\n"
         //        "w        %u\n"
         //        "h        %u\n", specX, specY, rgb, bmWidth, bmHeight );
                 
         //
         // user specifies position of baseline, adjust to top of bitmap
         //
         unsigned bmStartY = 0 ;
         int screenStartY = specY ;
         if( 0 > screenStartY )
         {
            bmStartY = ( 0-screenStartY );
            screenStartY = 0 ;
         } // starts off the screen, walk down in alphaMap

         unsigned bmStartX = 0 ;
         int screenStartX = specX ;
         if( 0 > screenStartX )
         {
            bmStartX = ( 0 - screenStartX );
            screenStartX  = 0 ;
         } // starts off the screen, walk right in alphaMap

         if( ( bmWidth > bmStartX ) 
             && 
             ( screenStartX <= imageWidth ) 
             && 
             ( screenStartY < imageHeight )
             &&
             ( bmStartY < bmWidth ) )
         {
            //
            // point row at start row/col
            //
            unsigned char *inRow = (unsigned char *)JS_GetStringBytes( sPixMap )
                                       + (bmWidth*bmStartY) + bmStartX ;
            unsigned const maxHeight = bmHeight-bmStartY ;
            unsigned const red   = (unsigned char)( rgb >> 16 );
            unsigned const green = (unsigned char)( rgb >> 8 );
            unsigned const blue  = (unsigned char)rgb ;
            fb.antialias( inRow,
                          bmWidth,
                          maxHeight,
                          screenStartX,
                          screenStartY,
                          screenStartX+bmWidth-1,
                          screenStartY+maxHeight-1,
                          red, green, blue,
                          imageMem, imageWidth, imageHeight, imageStride);
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
                        for( unsigned y = 0 ; y < (unsigned)height ; y++ )
                        {
                           for( unsigned x = 0 ; x < (unsigned)width ; x++ )
                           {
                              output[y+x*height] = pixMap[x+(height-y-1)*width];
                           }
                        }
      
                        break;
                     }
                  case rotate180_e :
                     {
                        for( unsigned y = 0 ; y < (unsigned)height ; y++ )
                        {
                           for( unsigned x = 0 ; x < (unsigned)width ; x++ )
                           {
                              output[y*width+x] = pixMap[((height-y-1)*width)+width-x-1];
                           }
                        }
                        break;
                     }
                  case rotate270_e :
                     {
                        for( unsigned y = 0 ; y < (unsigned)height ; y++ )
                        {
                           for( unsigned x = 0 ; x < (unsigned)width ; x++ )
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
    {"cropright",    jsCropRight,              1 },
    {"cropleft",     jsCropLeft,              1 },
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
  {"width",         ALPHAMAP_WIDTH,     JSPROP_ENUMERATE|JSPROP_PERMANENT },
  {"height",        ALPHAMAP_HEIGHT,    JSPROP_ENUMERATE|JSPROP_PERMANENT },
  {"pixBuf",        ALPHAMAP_PIXBUF,    JSPROP_ENUMERATE|JSPROP_PERMANENT },
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
         JS_ReportError( cx, "Error allocating alphaMap" );
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

