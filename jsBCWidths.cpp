/*
 * Module jsBCWidths.cpp
 *
 * This module defines the initialization routine and implementation
 * of the Javascript barcodeWidths() routine as declared and described
 * in jsBCWidths.h
 *
 * Implementation has a wrapper routine validate the symbology and
 * pass the details to the routine associated with the particular
 * symbology through a function table (if implemented).
 *
 * Change History : 
 *
 * $Log: jsBCWidths.cpp,v $
 * Revision 1.1  2004-05-07 13:32:58  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "jsBCWidths.h"
#include "jsBarcode.h"
#include <assert.h>
#include "js/jsobj.h"

#define dim( __arr ) ( sizeof( __arr )/sizeof( __arr[0] ) )

static char const usage[] = {
   "usage: barcodeWidths( symbology, encodeMe, desiredWidth )\n"
   "Use symbology from barcodeReader class\n"
};

static unsigned short const leftI2Of5[] = {
           // 1 1 1 1 1
   0x0028, // 0000101000, 0 nnwwn
   0x0202, // 1000000010, 1 wnnnw
   0x0082, // 0010000010, 2 nwnnw
   0x0280, // 1010000000, 3 wwnnn
   0x0022, // 0000100010, 4 nnwnw
   0x0220, // 1000100000, 5 wnwnn
   0x00a0, // 0010100000, 6 nwwnn
   0x000a, // 0000001010, 7 nnnww
   0x0208, // 1000001000, 8 wnnwn
   0x0088  // 0010001000, 9 nwnwn
};

static unsigned short const rightI2Of5[] = {
           //  1 1 1 1 1
   0x0014, // 0000010100, 0 nnwwn
   0x0101, // 0100000001, 1 wnnnw
   0x0041, // 0001000001, 2 nwnnw
   0x0140, // 0101000000, 3 wwnnn
   0x0011, // 0000010001, 4 nnwnw
   0x0110, // 0100010000, 5 wnwnn
   0x0050, // 0001010000, 6 nwwnn
   0x0005, // 0000000101, 7 nnnww
   0x0104, // 0100000100, 8 wnnwn
   0x0044  // 0001000100, 9 nwnwn
};

static JSBool i2of5( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;
   assert( ( 2 == argc ) && JSVAL_IS_STRING( argv[0] ) && JSVAL_IS_INT( argv[1] ) );
   JSString *sEncode = JSVAL_TO_STRING( argv[0] );
   if( sEncode )
   {
      unsigned desiredWidth = JSVAL_TO_INT( argv[1] );
      if( 0 < desiredWidth )
      {
         unsigned const inLength = JS_GetStringLength( sEncode );
         if( ( 0 < inLength ) && ( 0 == ( inLength & 1 ) ) )
         {
            char const preamble[] = {
               0, 0, 0, 0              // narrow, narrow, narrow, narrow
            };
            unsigned const preambleNarrow = 4 ;
            unsigned const preambleWide = 0 ;

            char const trailer[] = {
               1, 0, 0                 // wide, narrow, narrow
            };
            unsigned const trailerNarrow = 2 ;
            unsigned const trailerWide = 1 ;

            unsigned const numWidths = dim( preamble ) + dim( trailer ) + (5*inLength);
            unsigned const numNarrow = preambleNarrow + trailerNarrow + 3*inLength ;
            unsigned const numWide   = preambleWide + trailerWide + 2*inLength ;

            printf( "%u widths for %u digits, %u narrow, %u wide\n", numWidths, inLength, numNarrow, numWide );

            unsigned const minPixels = 2*numWide + numNarrow ;
            if( minPixels <= desiredWidth )
            {
               //
               // Determine what kind of multiplier to use, including ratio of
               // narrow to wide. 
               // 
               // The primary goal is to come as close to the desired width as
               // possible without going over with one of two ratios of narrow to wide
               //
               //    1:2
               //    2:5
               //
               // In order for the second option to even be available, we need enough
               // room for 
               //
               // The outputs from this section of code are wideWidth, narrowWidth, and totalWidth
               //               
               unsigned mult12 = desiredWidth / minPixels ;
               unsigned const total12 = mult12*minPixels ;
               unsigned totalWidth = total12 ;
               printf( "1:2 multiplier %u, total %u\n", mult12, total12 );

               unsigned wideWidth = 2*mult12 ;
               unsigned narrowWidth = mult12 ;

               unsigned const min25 = (2*numNarrow)+(5*numWide);
               if( min25 <= desiredWidth )
               {
                  unsigned mult25  = desiredWidth / min25 ;
                  unsigned total25 = mult25*min25 ;
                  printf( "2:5 multiplier %u, total %u\n", mult25, total25 );
                  if( total25 >= total12 )
                  {
                     printf( "Use 2:5 ratio %u >= %u >= %u\n", desiredWidth, total25, total12 );
                     wideWidth = 5*mult25 ;
                     narrowWidth = 2*mult25 ;
                     totalWidth = total25 ;
                  }
               }

               printf( "using widths %u:%u, total %u\n", narrowWidth, wideWidth, totalWidth );

               char *const outWidths = (char *)JS_malloc( cx, numWidths+ 1 );

               bool worked = true ;
               char const *inData = JS_GetStringBytes( sEncode );
               char *nextOut = outWidths ;
               
               for( unsigned pb = 0 ; pb < dim( preamble ); pb++ )
               {
                  if( preamble[pb] )
                  {
printf( "W" );
                     *nextOut++ = wideWidth ;
                  }
                  else
                  {
printf( "n" );
                     *nextOut++ = narrowWidth ;
                  }
               }
                  
               for( unsigned i = 0 ; i < inLength ; i += 2 )
               {
                  unsigned char const left  = *inData++ - '0' ;
                  unsigned char const right = *inData++ - '0' ;
                  if( ( left < 10 ) && ( right < 10 ) )
                  {
                     unsigned short w10 = leftI2Of5[left];
                     w10 |= rightI2Of5[right];
                     for( unsigned short m = 1 << 9 ; m > 0 ; m >>= 1 )
                     {
                        if( w10 & m )
                        {
                           *nextOut++ = wideWidth ;
                           printf( "W" );
                        }
                        else
                        {
                           *nextOut++ = narrowWidth ;
                           printf( "n" );
                        }
                     }
                  }
                  else
                  {
                     worked = false ;
                     break;
                  }
               }
               if( worked )
               {
                  for( unsigned pt = 0 ; pt < dim( trailer ); pt++ )
                  {
                     if( trailer[pt] )
                     {
printf( "W" );
                        *nextOut++ = wideWidth ;
                     }
                     else
                     {
printf( "n" );
                        *nextOut++ = narrowWidth ;
                     }
                  }
                     
                  JSObject *rObj = JS_NewObject( cx, &js_ObjectClass, 0, 0 );
printf( "rObj: %p\n", rObj );
                  *rval = OBJECT_TO_JSVAL( rObj ); // root

                  JSString *sWidths = JS_NewString( cx, outWidths, nextOut-outWidths );
printf( "sWidths: %p\n", sWidths );
                  JS_DefineProperty( cx, rObj, "widths", 
                                     STRING_TO_JSVAL( sWidths ), 
                                     0, 0, JSPROP_ENUMERATE );
                  JS_DefineProperty( cx, rObj, "length", INT_TO_JSVAL( totalWidth ), 0, 0, JSPROP_ENUMERATE );

               }
               else
                  *rval = JSVAL_FALSE ;
printf( "\n" );
            }
            else
               JS_ReportError( cx, "Desired size is too small: minimum is %u", minPixels );

         }
         else
            JS_ReportError( cx, "invalid i2of5 string, must have even # of digits" );
      }
      else
         JS_ReportError( cx, "Invalid i2of5 width request" );
   }
   else
      JS_ReportError( cx, "reading barcode string" );

   return JS_TRUE ;
}

static JSNative const symbologyFunctions[] = {
   0,           // BCR_NOSYM,         unknown symbology
   0,           // BCR_UPC,           UPC
   i2of5,       // BCR_I2OF5,         Interleaved 2 of 5
   0,           // BCR_CODE39,        Code 39 (alpha)
   0,           // BCR_CODE128,       Code 128
   0,           // BCR_EAN,           European Article Numbering System
   0,           // BCR_EAN128,           "        "        "        "
   0,           // BCR_CODABAR,       
   0,           // BCR_CODE93,        
   0,           // BCR_PHARMACODE,    
   0,           // BCR_NUMSYMBOLOGIES
};

static JSBool jsBarcodeWidths( JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval )
{
   *rval = JSVAL_FALSE ;

   if( ( 3 == argc )
       &&
       JSVAL_IS_INT( argv[0] )
       &&
       JSVAL_IS_STRING( argv[1] )
       &&
       JSVAL_IS_INT( argv[2] ) )
   {
      unsigned const symbology = JSVAL_TO_INT( argv[0] );
      if( symbology < BCR_NUMSYMBOLOGIES )
      {
         JSNative func = symbologyFunctions[symbology];
         if( func )
         {
            return func( cx, obj, argc-1, argv+1, rval );
         }
         else
            JS_ReportError( cx, "Unsupported symbology %s\n", bcrSymNames_[symbology] );
      }
      else
         JS_ReportError( cx, "Invalid symbology: %u\n", symbology );
   }
   else
      JS_ReportError( cx, usage );

   return JS_TRUE ;
}

static JSFunctionSpec functions[] = {
    {"barcodeWidths",       jsBarcodeWidths,       1 },
    {0}
};

bool initJSBCWidths( JSContext *cx, JSObject *glob )
{
   return JS_DefineFunctions( cx, glob, functions );
}

