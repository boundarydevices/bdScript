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
 * Revision 1.6  2008-10-16 00:10:31  ericn
 * [odomGraphics] Fix compiler warning
 *
 * Revision 1.5  2007-08-19 19:18:26  ericn
 * -[cleanup] remove unused (and obsolete) header
 *
 * Revision 1.4  2002/12/15 05:46:47  ericn
 * -Added support for horizontally scaled graphics based on input width
 *
 * Revision 1.3  2006/10/16 22:26:25  ericn
 * -added validate() method
 *
 * Revision 1.2  2006/08/16 21:22:14  ericn
 * -rename header
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
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
#include <assert.h>
#include <stdlib.h>
#include <math.h>

static char const *const imgFileNames[] = {
   "digitStrip.png"
,  "dollarSign.png"
,  "decimalPt.png"
,  "comma.png"
};

static unsigned const numImgFiles = sizeof(imgFileNames)/sizeof(imgFileNames[0]);

static char const scaleFileName[] = {
   "hscale.dat"
};

odomGraphics_t::odomGraphics_t(
   char const       *dirname,
   odometerMode_e    mode,
   unsigned          maxDigits,
   unsigned          maxWidth
)
   : maxDigits_(maxDigits)
   , sigToIndex_( new unsigned [maxDigits+1] )
   , digitPositions_( new unsigned const *[maxDigits+1] ) // worst case
   , dollarPositions_( new unsigned [maxDigits+1] )
   , comma1Positions_( new unsigned [maxDigits+1] )
   , comma2Positions_( new unsigned [maxDigits+1] )
   , decimalPositions_( new unsigned [maxDigits+1] )
   , digitStrip_( new fbImage_t *[maxDigits+1] )
   , dollarSign_( new fbImage_t *[maxDigits+1] )
   , decimalPoint_( new fbImage_t *[maxDigits+1] )
   , comma_( new fbImage_t *[maxDigits+1] )
   , worked_( false )
{
   char path[FILENAME_MAX];
   char *fname = stpcpy(path,dirname);
   if( '/' != fname[-1] )
      *fname++ = '/' ;
   //
   // load full-size images
   //
   fbImage_t **fbImages[numImgFiles] = {
      digitStrip_
    , dollarSign_
    , decimalPoint_
    , comma_ 
   };

   for( unsigned i = 0 ; i < numImgFiles ; i++ )
   {
      for( unsigned j = 0 ; j <= maxDigits ; j++ )
         fbImages[i][j] = 0 ;

      image_t img ;
      strcpy(fname, imgFileNames[i]);
      if( !imageFromFile( path, img ) ){
         perror( path );
         return ;
      }
      *fbImages[i] = new fbImage_t(img,imageMode(mode));
      printf( "%p\n", *fbImages[i] );
   }

   //
   // validate matching heights 
   //
   unsigned const digitHeight = digitStrip_[0]->height() / 10 ;
   if( dollarSign_[0]->height() != digitHeight ){
      fprintf( stderr, "Invalid dollar sign height %d/%d\n", dollarSign_[0]->height(), digitHeight );
      return ;
   }

   if( decimalPoint_[0]->height() != digitHeight ){
      fprintf( stderr, "Invalid decimal point height %d/%d\n", decimalPoint_[0]->height(), digitHeight );
      return ;
   }  

   if( comma_[0]->height() != digitHeight ){
      fprintf( stderr, "Invalid comma height %d/%d\n", comma_[0]->height(), digitHeight );
      return ;
   }

   assert( graphicsLayer != mode );
   assert( maxDigits >= 3 );
   unsigned minWidth = dollarSign_[0]->width()+(3*digitStrip_[0]->width())+decimalPoint_[0]->width();
   printf( "minWidth ($0.00) == %u\n", minWidth );
   unsigned biggest = minWidth 
                     + ((maxDigits-3)*digitStrip_[0]->width())
                     + ((maxDigits>5) ? comma_[0]->width() : 0 )
                     + ((maxDigits>8) ? comma_[0]->width() : 0 );
   printf( "maxWidth ($???,000.00) == %u\n", biggest );
   unsigned fsOverhead = dollarSign_[0]->width()+(2*comma_[0]->width())+decimalPoint_[0]->width();
   printf( "%u pixels of overhead (full-sized)\n", fsOverhead );
   unsigned fsSpace = maxWidth-fsOverhead ;
   unsigned fsDigits = fsSpace/digitStrip_[0]->width();
   printf( "space for %u full-sized digits(%u pixels at %u pix/digit)\n", fsDigits, fsSpace, digitStrip_[0]->width() );

   assert( maxWidth >= minWidth ); // re-work the graphics
   unsigned *const sigToIndex = (unsigned *)sigToIndex_ ;   // writable copy
   unsigned **const digitPositions = (unsigned **)digitPositions_ ; // writable
   unsigned *const dollarPositions = (unsigned *)dollarPositions_ ; // writable
   unsigned *const comma1Positions = (unsigned *)comma1Positions_ ; // writable
   unsigned *const comma2Positions = (unsigned *)comma2Positions_ ; // writable
   unsigned *const decimalPositions = (unsigned *)decimalPositions_ ; // writable

   memset( sigToIndex, 0, (maxDigits+1)*sizeof(sigToIndex[0]) );
   memset( digitPositions, 0, (maxDigits+1)*sizeof(digitPositions[0]) );
   memset( dollarPositions, 0, (maxDigits+1)*sizeof(dollarPositions[0]) );
   memset( comma1Positions, 0, (maxDigits+1)*sizeof(comma1Positions[0]) );
   memset( comma2Positions, 0, (maxDigits+1)*sizeof(comma2Positions[0]) );
   memset( decimalPositions, 0, (maxDigits+1)*sizeof(decimalPositions[0]) );

   unsigned numScaled = 2 ;
   unsigned maxScaled = (maxDigits-3);

   strcpy( fname, scaleFileName );
   FILE *fIn = fopen( path, "rt" );
   if( fIn ){
      char inBuf[80];
      if( fgets( inBuf, sizeof(inBuf), fIn ) ){
         numScaled = strtoul(inBuf, 0, 0 );
         if( maxScaled < numScaled ){
            fprintf( stderr, "Invalid scale count(%u) in %s\n", numScaled, path );
            numScaled = maxScaled ;
         }
      }
      fclose( fIn );
   }
   else
      perror( path );

   if( biggest > maxWidth ){
      double scale = (1.0*maxWidth)/biggest ;
      double scaleAdder = (1.0-scale)/(numScaled-1);
      for( unsigned i = 1 ; i < numScaled ; i++ ){
         unsigned idx = numScaled - i ;
         printf( "scale[%u] == %f\n", idx, scale );
         for( unsigned j = 0 ; j < numImgFiles ; j++ ){
            fbImage_t const &src = **fbImages[j];
            unsigned origWidth = src.width();
            unsigned newWidth = (unsigned)(floor(scale*origWidth));
            printf( "%s: %u->%u\n", imgFileNames[j], origWidth, newWidth );
            fbImages[j][idx] = src.scaleHorizontal( newWidth );
         }
         unsigned const overhead = dollarSign_[idx]->width()+(2*comma_[idx]->width())+decimalPoint_[idx]->width();
         unsigned digitSpace = maxWidth-overhead ;
         unsigned digitsThisSize = digitSpace/digitStrip_[idx]->width();
         printf( "overhead[%u] == %u, %u digits (%u pixels for digits/%u pixels/digit)\n", 
                 idx, overhead, digitsThisSize, digitSpace, digitStrip_[idx]->width() );
         while( digitsThisSize > fsDigits ){
            sigToIndex[digitsThisSize--] = idx ;
         }

         scale += scaleAdder ;
      }
   }
   else {
      numScaled = 1 ;
   }

   //
   // produce digit positions for each scaling
   //
   for( unsigned i = 0 ; i < numScaled ; i++){
printf( "scaling #%u\n", i ); 
      int rightEdge = maxWidth ;
      unsigned *positions = new unsigned [maxDigits];
      unsigned const digitWidth = digitStrip_[i]->width();
      unsigned const dollarWidth = dollarSign_[i]->width();
      unsigned j ;
      for( j = 0 ; j < maxDigits ; j++ ){
         if( 2 == j ){
            rightEdge -= decimalPoint_[i]->width();
            decimalPositions[i] = rightEdge ;
printf( "   decimal at %u\n", rightEdge );
         }
         else if( 5 == j ){
            rightEdge -= comma_[i]->width();
            comma1Positions[i] = rightEdge ;
printf( "   comma1 at %u\n", rightEdge );
         }
         else if( 8 == j ){
            rightEdge -= comma_[i]->width();
            comma2Positions[i] = rightEdge ;
printf( "   comma2 at %u\n", rightEdge );
         }
         rightEdge -= digitWidth ;
         if( dollarWidth <= (unsigned)rightEdge ){
            positions[j] = rightEdge ;
         }
         else
            break ;
      }
      printf( "%u significant digits for scaling %u\n", j, i );
      digitPositions[i] = positions ;
   }

   //
   // get dollar sign position for each significant digit 
   //
   
   for( unsigned i = 0 ; i <= maxDigits_ ; i++ ){
      unsigned scaleIdx = sigToIndex_[i];
      unsigned const digitWidth = getStrip(i).width();
      unsigned left = maxWidth-(i*digitWidth);
      if( i > 2 ){
         left -= decimalPoint_[scaleIdx]->width();
         if( i > 5 ){
            unsigned const commaWidth = comma_[scaleIdx]->width();
            left -= commaWidth ;
            if( i > 8 )
               left -= commaWidth ;
         }
      }
      left -= dollarSign_[scaleIdx]->width();
      dollarPositions[i] = left ;
printf( "dollarPos[%u] == %u (%u digit, %u comma, %u decimal, %u dollar)\n", i, left,
        digitWidth, comma_[scaleIdx]->width(), decimalPoint_[scaleIdx]->width(), dollarSign_[scaleIdx]->width() );
   }
   
   numScaled_ = numScaled ;

   worked_ = true ;
}

odomGraphics_t::~odomGraphics_t(void)
{
   delete [] sigToIndex_ ;

   for( unsigned i = 0 ; i < numScaled_ ; i++ ){
      unsigned const *positions = digitPositions_[i];
      if( positions ){
         delete [] positions ;
      }
   }
   delete [] digitPositions_ ;

   fbImage_t **fbImages[numImgFiles] = {
      digitStrip_
    , dollarSign_
    , decimalPoint_
    , comma_ 
   };

   for( unsigned i = 0 ; i < numImgFiles ; i++ ){
      for( unsigned j = 0 ; j < numScaled_ ; j++ ){
         fbImage_t *img = fbImages[i][j];
         if( img )
            delete img ;
      }
   }
}

bool odomGraphics_t::validate() const
{
   bool valid = true ;
   
   fbImage_t **fbImages[numImgFiles] = {
      digitStrip_
    , dollarSign_
    , decimalPoint_
    , comma_ 
   };
   for( unsigned i = 0 ; valid && (i < numImgFiles) ; i++ ){
      for( unsigned j = 0 ; valid && ( j < numScaled_ ) ; j++ ){
         fbImage_t *img = fbImages[i][j];
         if( img ){
            valid = img->validate();
            if( !valid )
               printf( "Invalid image: %s\n", imgFileNames[i] );
         }
      }
   }
   return valid ;
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

#ifdef MODULETEST

#include <stdio.h>
#include <ctype.h>
#include "sm501alpha.h"

static unsigned windowWidth = getFB().getWidth();
static unsigned digits = 10 ;

static void parseArgs( int &argc, char const **argv )
{
   for( int arg = 1 ; arg < argc -1 ; arg++ ){
      char const *param = argv[arg];
      if( '-' == *param ){
         if( 'w' == tolower(param[1]) ){
            windowWidth = strtoul(argv[arg+1],0,0);
            for( int fwd = arg+2 ; fwd < argc ; fwd++ ){
               argv[fwd-2] = argv[fwd];
            }
            argc -= 2 ;
            arg-- ;
         } // width command
         else if( 'd' == tolower(param[1]) ){
            digits = strtoul(argv[arg+1],0,0);
            for( int fwd = arg+2 ; fwd < argc ; fwd++ ){
               argv[fwd-2] = argv[fwd];
            }
            argc -= 2 ;
            arg-- ;
         } // 
      }
   }
}


int main( int argc, char const * argv[] )
{
   parseArgs(argc,argv);

   if( 1 < argc ){
      printf( "width: %u, %u digits\n", windowWidth, digits );
      unsigned numGraphics = argc-1 ;
      odomGraphics_t **graphics = new odomGraphics_t *[numGraphics];
      unsigned xPos = 0 ;
      sm501alpha_t &alpha = sm501alpha_t::get( sm501alpha_t::rgba4444 );
      for( int arg = 1 ; arg < argc ; arg++ ){
         char const *dirName = argv[arg];
         odomGraphics_t *g = new odomGraphics_t( argv[arg], alphaLayer, digits, windowWidth );
         if( g->validate() ){
            printf( "graphics %s loaded\n", dirName );
            graphics[arg-1] = g ;
            unsigned const * const sigToIndex = g->sigToIndex();
            unsigned prevIndex = g->numScaled();
            for( unsigned i = 0 ; i <= digits ; i++ ){
               unsigned const idx = sigToIndex[i];
               printf( "[%u] == %u\n", i, idx );
               if( idx != prevIndex ){
                  prevIndex = idx ;
                  fbImage_t const &strip = g->getStrip(i);
                  alpha.draw4444((unsigned short *)strip.pixels(), xPos, 0, strip.width(), strip.height() );
                  xPos += strip.width();
                  fbImage_t const &dollar = g->getDollar(i);
                  alpha.draw4444((unsigned short *)dollar.pixels(), xPos, 0, dollar.width(), dollar.height() );
                  xPos += dollar.width();
                  printf( "   " );
                  for( unsigned j = 0 ; j < digits ; j++ ){
                     printf( "[%u:%u] ", j, g->digitOffset(i,j) );
                  }
                  printf( "\n" );
               }
            }
            char inBuf[80];
            fgets( inBuf, sizeof(inBuf), stdin );
         }
         else {
            fprintf( stderr, "Error loading graphics from %s\n", dirName );
            return -1 ;
         }
      }
      printf( "loaded %u sets of graphics\n", numGraphics );

      for( unsigned i = 0 ; i < numGraphics ; i++ ){
         delete graphics[i];
      }
   }
   else
      fprintf( stderr, "Usage: %s directory [-w #] [-d #]\n", argv[0] );

   return 0 ;
}

#endif
