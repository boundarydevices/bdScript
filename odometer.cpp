/*
 * Program odometer.cpp
 *
 * This program displays an odometer
 *
 *
 * Change History : 
 *
 * $Log: odometer.cpp,v $
 * Revision 1.2  2006-06-10 16:31:00  ericn
 * -digitInfo->videoGraphics rename, save ending image
 *
 * Revision 1.1  2006/06/06 03:04:32  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include <stdio.h>
#include "fbDev.h"
#include "imgFile.h"
#include "ftObjs.h"
#include "memFile.h"
#include "imgToPNG.h"
#include <signal.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>
#include <execinfo.h>
#include "fbImage.h"

static bool volatile doExit = false ;
static bool volatile drawing = false ;
static bool volatile flipping = false ;

static unsigned numDrawing_ = 0 ;
static unsigned numFlipping_ = 0 ;
static unsigned totalTicks_ = 0 ;

static void tick( int signo )
{
   ++totalTicks_ ;
   if( drawing )
      ++numDrawing_ ;
   else if( flipping )
      ++numFlipping_ ;
}

static void printBackTrace()
{
   void *btArray[128];
   int const btSize = backtrace(btArray, sizeof(btArray) / sizeof(void *) );
   fprintf( stderr, "########## Backtrace ##########\n"
                    "Number of elements in backtrace: %u\n", btSize );

   if (btSize > 0)
      backtrace_symbols_fd( btArray, btSize, fileno(stderr) );
}

static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   printf( drawing ? "drawing\n" : "not drawing\n" );
   printf( flipping ? "flipping\n" : "not flipping\n" );

   printBackTrace();

   doExit = true ;
}

struct point_t {
   unsigned x ;
   unsigned y ;
};

inline void rgbParts( unsigned long rgb,
            unsigned    &r,
            unsigned     &g,
            unsigned    &b )
{
   r = rgb >> 16 ;
   g = ( rgb >> 8 ) & 0xff ;
   b = rgb & 0xff ;
}

inline unsigned short make16( unsigned r, unsigned g, unsigned b )
{
   return ((r>>3)<<11) | ((g>>2)<<5) | (b>>3);
}

static unsigned short mix( 
   unsigned short in16,
   unsigned char  opacity,
   unsigned char  highlight )
{
   unsigned short b = (((in16 & 0x1f))*opacity)/255;
   in16 >>= 5 ;
   unsigned short g = (((in16 & 0x3f))*opacity)/255;
   in16 >>= 6 ;
   unsigned short r = (((in16 & 0x1f))*opacity)/255;
   
   if( highlight )
   {
      // diffs from white
      unsigned short ldb = (0x1f-b);
      b += (ldb*highlight)/255 ;
      unsigned short ldg = (0x3f-g);
      g += (ldg*highlight)/255 ;
      unsigned short ldr = (0x1f-r);
      r += (ldr*highlight)/255 ;
   }
   
   unsigned short out16 = (r<<11)|(g<<5)|b ;
   return out16 ;
}

static void drawDigit( 
   fbDevice_t &fb
,  unsigned x
,  unsigned y
,  unsigned digitHeight
,  unsigned short const *pixels
,  unsigned pixelWidth
,  unsigned pixelHeight
,  unsigned offset
,  unsigned char const *darkGrad
,  unsigned char const *lightGrad
)
{
   static bool first = true ;

   drawing = true ;
   unsigned short const *bottomRow = pixels + pixelWidth*pixelHeight ;
   unsigned short *outRow = fb.getRow(y) + x ;
   unsigned short const *inRow = pixels + (offset*pixelWidth);

   for( unsigned y = 0 ; !doExit && ( y < digitHeight ) ; y++ ) {
      unsigned char o = darkGrad[y];
      unsigned char l = lightGrad[y];
      unsigned short *nextOut = outRow ;
      outRow += fb.getWidth();
      unsigned short const *nextIn = inRow ;
      inRow += pixelWidth ;
      if( bottomRow <= inRow )
         inRow = pixels ;
   
      for( unsigned x = 0 ; x < pixelWidth ; x++ ){
         unsigned short in = *nextIn++ ;
         unsigned short out16 = mix(in,o,l);
         *nextOut++ = out16 ;
      }
   }
   
   if( first ){
//      printBackTrace();
      first = false ;
   }
   drawing = false ;
}

struct valueGraphics_t {
   image_t  background_ ;
   image_t  digitStrip_ ;
   image_t  dollarSign_ ;
   image_t  decimalPoint_ ;
   image_t  comma_ ;
   unsigned char  *shadow_ ;
   unsigned char  *highlight_ ;

   valueGraphics_t(void){ memset(this,0,sizeof(*this)); }
   ~valueGraphics_t(void);
private:
   valueGraphics_t(valueGraphics_t const &);
};

valueGraphics_t::~valueGraphics_t( void )
{
   if( shadow_ )
      delete [] shadow_ ;
   if( highlight_ )
      delete [] highlight_ ;
}

static char const *const imgFileNames[] = {
   "background.png"
,  "digitStrip.png"
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

static bool loadDigitInfo( 
   char const  *directory,
   valueGraphics_t &info 
)
{
   char path[FILENAME_MAX];
   char *fname = stpcpy(path,directory);
   if( '/' != fname[-1] )
      *fname++ = '/' ;

   //
   // load images
   //
   image_t *const images[numImgFiles] = {
      &info.background_,
      &info.digitStrip_,
      &info.dollarSign_,
      &info.decimalPoint_,
      &info.comma_
   };
   
   for( unsigned i = 0 ; i < numImgFiles ; i++ )
   {
      strcpy(fname, imgFileNames[i]);
      if( !imageFromFile( path, *images[i] ) ){
         perror( path );
         return false ;
      }
   }
   
   //
   // validate matching heights 
   //
   unsigned const digitHeight = info.digitStrip_.height_ / 10 ;

   if( info.dollarSign_.height_ != digitHeight ){
      fprintf( stderr, "Invalid dollar sign height %d/%d\n", info.dollarSign_.height_, digitHeight );
      return false ;
   }

   if( info.decimalPoint_.height_ != digitHeight ){
      fprintf( stderr, "Invalid decimal point height %d/%d\n", info.decimalPoint_.height_, digitHeight );
      return false ;
   }

   if( info.comma_.height_ != digitHeight ){
      fprintf( stderr, "Invalid comma height %d/%d\n", info.comma_.height_, digitHeight );
      return false ;
   }

   //
   // load gradient files
   //
   unsigned char **gradients[numGradientFiles] = {
      &info.shadow_
   ,  &info.highlight_
   };
   
   for( unsigned i = 0 ; i < numGradientFiles ; i++ ){
      strcpy( fname, gradientFileNames[i] );
      memFile_t fgrad( path );
      if( !fgrad.worked() ){
         perror( path );
         return false ;
      }
   
      if( fgrad.getLength() != digitHeight ){
         fprintf(stderr, "gradient file %s should be %u bytes long, not %u\n",
            path, digitHeight, fgrad.getLength() );
         return false ;
      }
      
      *gradients[i] = new unsigned char[fgrad.getLength()];
      memcpy( *gradients[i], fgrad.getData(), fgrad.getLength() );
   
   }
   return true ;
}

struct valueInfo_t {
   valueInfo_t( valueGraphics_t const &vg,
                point_t         const &pt,
                unsigned               maxDigits );
   ~valueInfo_t( void );

   void setTarget( unsigned v );
   void draw( void );
   rectangle_t const &getRect() const { return rect_ ; }
   
   valueGraphics_t const &vg_ ;
   unsigned const     maxDigits_ ;
   unsigned const     x_ ;
   unsigned const     y_ ;
   bool const         needComma_ ;
   unsigned const     totalDigits_ ; // including dollar sign, decimal point and thousands separator
   unsigned const     xRight_ ;
   image_t            background_ ;
   unsigned           target_ ;
   unsigned           value_ ;
   unsigned long      pixelTarget_ ;
   unsigned long      pixelValue_ ;
   unsigned long      velocity_ ;
   rectangle_t        rect_ ;
   unsigned           commaPos_ ;
   unsigned           dollarPos_ ;
   unsigned           decimalPos_ ;
   char               sTargetValue_[11];
   char               sValue_[11];
private:
   valueInfo_t( valueInfo_t const & );

};

valueInfo_t::valueInfo_t( 
   valueGraphics_t const &vg,
   point_t     const     &pt,
   unsigned               maxDigits )
   : vg_(vg)
   , maxDigits_(maxDigits)
   , x_(pt.x)
   , y_(pt.y)
   , needComma_( 5<maxDigits )
   , totalDigits_(2+maxDigits+needComma_)
   , xRight_( pt.x 
             + maxDigits*vg.digitStrip_.width_
             + vg.decimalPoint_.width_
             + vg.dollarSign_.width_
             + ( needComma_ ? vg.comma_.width_ : 0 ) )
   , target_(0)
   , value_(0)
   , pixelTarget_(0)
   , pixelValue_(0)
   , velocity_(0)
   , rect_()
   , commaPos_( -1U )
   , dollarPos_( -1U )
   , decimalPos_( -1U )
{
   rect_.xLeft_  = pt.x ;
   rect_.yTop_   = pt.y ;
   rect_.width_  = xRight_ - pt.x ;
   rect_.height_ = vg.decimalPoint_.height_ ;
}

valueInfo_t::~valueInfo_t( void )
{
}

void valueInfo_t::setTarget( unsigned v )
{
   target_ = v ;
   pixelTarget_ = v*rect_.height_ ;
   
   if( v < value_ )
   {
      value_ = 0 ;
      pixelValue_ = 0 ;
   }
   snprintf( sTargetValue_, sizeof(sTargetValue_), "%010u", target_ );

   velocity_    = ( pixelTarget_ - pixelValue_ ) / 60 ;
//   velocity_ = 1 ;

}

void valueInfo_t::draw( void )
{
   if( pixelTarget_ != pixelValue_ )
   {
      fbDevice_t &fb = getFB();

      unsigned x = xRight_ ;
      unsigned digitNum = 0 ;
      
      unsigned long pValue = pixelValue_ + velocity_ ;
      if( pValue > pixelTarget_ )
         pValue = pixelTarget_ ;

// printf( "%lu->%lu\n", pValue, pixelTarget_ );

      pixelValue_ = pValue ;
      value_ = pValue / rect_.height_ ;
      
//      snprintf( sValue_, sizeof(sValue_), "%010u", value_ );
      
//      printf( "%s:%s\n", sTargetValue_, sValue_ );      
      unsigned v = value_ ;

      while( ( 3 > digitNum )
             ||
             ( 0 < v ) )
      {
//         unsigned char dig = v % 10 ;
//         printf( "%u", dig );
         v /= 10 ;
      
         x -= vg_.digitStrip_.width_ ;
unsigned const pixOffs = pValue % vg_.digitStrip_.height_ ;
//fprintf( stderr, "dig %d, pixOffs %u\n", dig, pixOffs );
         drawDigit( fb, x, y_, 
                    rect_.height_,
                    (unsigned short *)vg_.digitStrip_.pixData_,
                    vg_.digitStrip_.width_,
                    vg_.digitStrip_.height_,
                    pixOffs,
                    vg_.shadow_,
                    vg_.highlight_ );
//         pValue /= vg_.digitStrip_.height_ ;
         pValue /= 10 ;

         digitNum++ ;
         if( 2 == digitNum )
         {
//            printf( "." );
            x -= vg_.decimalPoint_.width_ ;
            if( decimalPos_ != x )
            {
               decimalPos_ = x ;
               drawDigit( fb, x, y_, 
                          vg_.decimalPoint_.height_,
                          (unsigned short *)vg_.decimalPoint_.pixData_,
                          vg_.decimalPoint_.width_,
                          vg_.decimalPoint_.height_,
                          0,
                          vg_.shadow_,
                          vg_.highlight_ );
            }
         }
         else if( ( 5 == digitNum ) && ( 0 < v ) )
         {
//            printf( "," );
            x -= vg_.comma_.width_ ;
            if( commaPos_ != x )
            {
               commaPos_ = x ;
               drawDigit( fb, x, y_, 
                          vg_.comma_.height_,
                          (unsigned short *)vg_.comma_.pixData_,
                          vg_.comma_.width_,
                          vg_.comma_.height_,
                          0,
                          vg_.shadow_,
                          vg_.highlight_ );
            }
         }
         
         velocity_ = ( pixelTarget_ - pixelValue_ ) / 60 ;
         if( ( 0 == velocity_ )
             && 
             ( pixelTarget_ > pixelValue_ ) )
           velocity_ = 1 ;
      }
      
      x -= vg_.dollarSign_.width_ ;
      if( dollarPos_ != x )
      {
         dollarPos_ = x ;
         drawDigit( fb, x, y_, 
                    vg_.dollarSign_.height_,
                    (unsigned short *)vg_.dollarSign_.pixData_,
                    vg_.dollarSign_.width_,
                    vg_.dollarSign_.height_,
                    0,
                    vg_.shadow_,
                    vg_.highlight_ );
      }

//      printf( "\n" );
   }
}

unsigned const maxDigits = 8 ;

int main( int argc, char const *const argv[] )
{
   //
   // odd number of parameters > 1
   //
   if( ( 1 < argc ) && ( 1 == ( argc & 1 ) ) ) {
      fbDevice_t &fb = getFB();

      valueGraphics_t vg ;
      if( !loadDigitInfo( "/tmp", vg ) )
         return -1 ;

      unsigned const numValues = (argc-1)/2 ;
 
      printf( "%u values\n", numValues );
 
      rectangle_t *const rects = new rectangle_t [numValues+1];
      point_t *const points = new point_t[numValues];
      valueInfo_t ** const values = new valueInfo_t *[numValues];
      for( unsigned i = 0 ; i < numValues ; i++ ){
         points[i].x = strtoul(argv[1+i*2],0,0);
         points[i].y = strtoul(argv[2+i*2],0,0);
         values[i] = new valueInfo_t(vg, points[i], maxDigits);

         values[i]->setTarget( (i+1)*12345678 );
printf( "value %u at %u:%u\n", i, points[i].x, points[i].y );         
         rects[i] = values[i]->rect_ ;         
      }
      
      memset( rects+numValues, 0, sizeof(rects[numValues]) );

      //
      // Initialize screen
      //
//      fb.doubleBuffer();

      fb.clear(0xFF, 0xFF, 0xFF);
      fb.render( 0, 0, 
            vg.background_.width_,
            vg.background_.height_,
            (unsigned short const *)vg.background_.pixData_ );

      for( unsigned i = 0 ; i < numValues ; i++ ) {
         values[i]->draw();
      }

      signal( SIGINT, ctrlcHandler );
      signal( SIGVTALRM, tick );

      struct itimerval timer;

      memset( &timer, 0, sizeof(timer) );
      /* Configure the timer to expire after 250 msec... */
      timer.it_value.tv_usec    =
      timer.it_interval.tv_usec = 250000;
      setitimer (ITIMER_VIRTUAL, &timer, NULL);
   
      unsigned iterations = 0 ;
      
      unsigned long vsyncStart ;
      fb.syncCount(vsyncStart);
      time_t startTick = time(0);
      while( !doExit )
      {
         unsigned numSteady = 0 ;
         for( unsigned i = 0 ; !doExit && ( i < numValues ) ; i++ ){
            values[i]->draw();
            numSteady += ( values[i]->pixelValue_ == values[i]->pixelTarget_ );
         }
         ++iterations ;
         
         flipping = true ;
//         fb.flip(); // rects);
         flipping = false ;
         
         if( numSteady == numValues )
            break ;
      }
      unsigned long vsyncEnd ;
      fb.syncCount(vsyncEnd);
   
      printf( "%u iterations, %lu refreshes in %lu seconds\n",
         iterations, vsyncEnd-vsyncStart, time(0)-startTick );
      printf( "%u ticks, %u drawing, %u flipping, %u other\n",
              totalTicks_, numDrawing_, numFlipping_, totalTicks_-numDrawing_-numFlipping_ );
              
      if( !doExit ){
         image_t screenImg ;
         screenImageRect( fb, values[0]->getRect(), screenImg );

         void const *pngData ;
         unsigned    pngSize ;
         if( imageToPNG( screenImg, pngData, pngSize ) ){
            printf( "%u bytes of png\n", pngSize );
            char const outFileName[] = {
               "/tmp/odomEnd.png"
            };
            FILE *fOut = fopen( outFileName, "wb" );
            if( fOut )
            {
               fwrite( pngData, 1, pngSize, fOut );
               fclose( fOut );
            }
            else
               perror( outFileName );
            free((void *)pngData);
         }
      }
   }
   else
      fprintf( stderr, "Usage: %s x y [x y...]\n", argv[0] );
   return 0 ;
}
