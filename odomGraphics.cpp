/*
 * Module odomGraphics.cpp
 *
 * This module defines the methods of the odomGraphics_t
 * class as declared in odomGraphics.h
 *
 *
 * Change History : 
 *
 * $Log: odomGraphics.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomGraphics.h"
#include <string.h>
#include <stdio.h>
#include "imgFile.h"
#include "memFile.h"
#include "fbImage.h"
#include "odomHighlight2.h"

static char const *const imgFileNames[] = {
   "digitStrip.png"
,  "dollarSign.png"
,  "decimalPt.png"
,  "comma.png"
};

static unsigned const numImgFiles = sizeof(imgFileNames)/sizeof(imgFileNames[0]);

static char const *const gradientFileNames[] = {
   "vshadow.dat"
,  "vhighlight.dat"
};

static unsigned const numGradientFiles = sizeof(gradientFileNames)/sizeof(gradientFileNames[0]);

odomGraphics_t::odomGraphics_t(
   char const       *dirname,
   odometerMode_e    mode 
)
   : worked_( false )
{
   char path[FILENAME_MAX];
   char *fname = stpcpy(path,dirname);
   if( '/' != fname[-1] )
      *fname++ = '/' ;

   //
   // load images
   //
   fbImage_t *const fbImages[numImgFiles] = {
      &digitStrip_ 
    , &dollarSign_ 
    , &decimalPoint_ 
    , &comma_ 
   };

   for( unsigned i = 0 ; i < numImgFiles ; i++ )
   {
      image_t img ;
      strcpy(fname, imgFileNames[i]);
      if( !imageFromFile( path, img ) ){
         perror( path );
         return ;
      }
      *fbImages[i] = fbImage_t(img,imageMode(mode));
   }

   //
   // validate matching heights 
   //
   unsigned const digitHeight = digitStrip_.height() / 10 ;

   if( dollarSign_.height() != digitHeight ){
      fprintf( stderr, "Invalid dollar sign height %d/%d\n", dollarSign_.height(), digitHeight );
      return ;
   }

   if( decimalPoint_.height() != digitHeight ){
      fprintf( stderr, "Invalid decimal point height %d/%d\n", decimalPoint_.height(), digitHeight );
      return ;
   }  

   if( comma_.height() != digitHeight ){
      fprintf( stderr, "Invalid comma height %d/%d\n", comma_.height(), digitHeight );
      return ;
   }

   if( graphicsLayer == mode ){
      //
      // load gradient files
      //
      unsigned char *shadow = 0 ;
      unsigned char *highlight = 0 ;
      unsigned char **gradients[numGradientFiles] = {
         &shadow
       , &highlight
      };
      
      for( unsigned i = 0 ; i < numGradientFiles ; i++ ){
         strcpy( fname, gradientFileNames[i] );
         memFile_t fgrad( path );
         if( !fgrad.worked() ){
            perror( path );
            return ;
         }
      
         if( fgrad.getLength() != digitHeight ){
            fprintf(stderr, "gradient file %s should be %u bytes long, not %u\n",
               path, digitHeight, fgrad.getLength() );
            return ;
         }
         
         *gradients[i] = new unsigned char[fgrad.getLength()];
         memcpy( *gradients[i], fgrad.getData(), fgrad.getLength() );
      }
   
      digitAlpha_ = fbPtr_t( digitHeight*(digitStrip_.width()) );
      createHighlight( shadow, 
                       highlight,
                       digitHeight,
                       digitStrip_.width(),
                       (unsigned char *)digitAlpha_.getPtr() );
   
      dollarAlpha_ = fbPtr_t( digitHeight*(dollarSign_.width()) );
      createHighlight( shadow, 
                       highlight,
                       digitHeight,
                       dollarSign_.width(),
                       (unsigned char *)dollarAlpha_.getPtr() );
   
      decimalAlpha_ = fbPtr_t( digitHeight*(decimalPoint_.width()) );
      createHighlight( shadow, 
                       highlight,
                       digitHeight,
                       decimalPoint_.width(),
                       (unsigned char *)decimalAlpha_.getPtr() );
   
      commaAlpha_ = fbPtr_t( digitHeight*(comma_.width()) );
      createHighlight( shadow, 
                       highlight,
                       digitHeight,
                       comma_.width(),
                       (unsigned char *)commaAlpha_.getPtr() );
   
      delete [] shadow ;
      delete [] highlight ;
   }
   else {
   }

   worked_ = true ;
}

odomGraphics_t::~odomGraphics_t(void)
{
}

#include <map>
#include <string>

static std::map<std::string,odomGraphics_t *> graphByName_ ;

static odomGraphicsByName_t *inst_ = 0 ;

odomGraphicsByName_t &odomGraphicsByName_t::get(void)
{
   if( 0 == inst_ )
      inst_ = new odomGraphicsByName_t ;
   return *inst_ ;
}

void odomGraphicsByName_t::add( char const *name, odomGraphics_t *graphics )
{
   graphByName_[name] = graphics ;
}

odomGraphics_t const *odomGraphicsByName_t::get(char const *name)
{
   return graphByName_[name];
}

odomGraphicsByName_t::odomGraphicsByName_t( void ){
}


