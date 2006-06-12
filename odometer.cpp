/*
 * Program odometer.cpp
 *
 * This program displays an odometer
 *
 *
 * Change History : 
 *
 * $Log: odometer.cpp,v $
 * Revision 1.3  2006-06-12 13:04:11  ericn
 * -rework drawing loop, before palettizing
 *
 * Revision 1.2  2006/06/10 16:31:00  ericn
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
#include <unistd.h>
#include <assert.h>
#include <execinfo.h>
#include <alloca.h>
#include "fbImage.h"
#include <set>

#define TICKSTOTARGET 60

static bool volatile doExit = false ;
static bool volatile drawing = false ;
static bool volatile mixing = false ;
static bool volatile flipping = false ;

static unsigned numDrawing_ = 0 ;
static unsigned numMixing_ = 0 ;
static unsigned numFlipping_ = 0 ;
static unsigned totalTicks_ = 0 ;
static bool volatile increment_ = false ;

static void tick( int signo )
{
   ++totalTicks_ ;
   if( 7 == ( totalTicks_ & 7 ) )
      increment_ = true ;
   if( drawing )
      ++numDrawing_ ;
   else if( flipping )
      ++numFlipping_ ;
      
   if( mixing )
      ++numMixing_ ;
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
/*   
   return in16 ;
*/
   unsigned short b = (((in16 & 0x1f))*opacity)/255;
   in16 >>= 5 ;
   unsigned short g = (((in16 & 0x3f))*opacity)/255;
   in16 >>= 6 ;
   unsigned short r = (((in16 & 0x1f))*opacity)/255;

   // diffs from white
   unsigned short ldb = (0x1f-b);
   b += (ldb*highlight)/255 ;
   unsigned short ldg = (0x3f-g);
   g += (ldg*highlight)/255 ;
   unsigned short ldr = (0x1f-r);
   r += (ldr*highlight)/255 ;

   unsigned short out16 = (r<<11)|(g<<5)|b ;
   return out16 ;
}

static void highlight( 
   image_t &img
,  unsigned char const *darkGrad
,  unsigned char const *lightGrad
)
{
   unsigned short *pixels = (unsigned short *)img.pixData_ ;
   for( unsigned y = 0 ; y < img.height_ ; y++ ) {
      unsigned char o = darkGrad[y];
      unsigned char l = lightGrad[y];
      for( unsigned x = 0 ; x < img.width_ ; x++ ){
         unsigned short in = *pixels ;
         unsigned short out16 = mix(in,o,l);
         *pixels++ = out16 ;
      }
   }
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
//   unsigned short *const lineOut = (unsigned short *)alloca(pixelWidth*2);
   
   for( unsigned y = 0 ; !doExit && ( y < digitHeight ) ; y++ ) {
      unsigned char o = darkGrad[y];
      unsigned char l = lightGrad[y];
      unsigned short *nextOut = outRow ;
      outRow += fb.getWidth();
      unsigned short const *nextIn = inRow ;
      __asm__ volatile (
         "  pld   [%0, #0]\n"
         "  pld   [%0, #32]\n"
         "  pld   [%0, #64]\n"
         "  pld   [%0, #96]\n"
         : 
         : "r" (nextIn)
      );
      inRow += pixelWidth ;
      if( bottomRow <= inRow )
         inRow = pixels ;
      
      for( unsigned x = 0 ; x < pixelWidth ; x++ ){
         unsigned short in = *nextIn++ ;
         mixing = true ;
         unsigned short out16 = mix(in,o,l);
         mixing = false ;
         *nextOut++ = out16 ;
      }
   }

   if( first ){
//      printBackTrace();
      first = false ;
   }
   drawing = false ;
}


//
// Load all of the graphic (static) parts of an odometer value
//
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

static bool loadValueGraphics( 
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

   std::set<unsigned short> shadeComb ;
   for( unsigned i = 0 ; i < digitHeight ; i++ )
   {
      unsigned short highShadow = ( ((unsigned)info.highlight_[i]) << 8 )
                                  | info.shadow_[i];
      shadeComb.insert(highShadow);
   }
   printf( "%u combinations of highlight and shadow\n", shadeComb.size() );

   //
   // pre-highlight symbol images
   //
   highlight( info.dollarSign_, info.shadow_, info.highlight_ );
   highlight( info.decimalPoint_, info.shadow_, info.highlight_ );
   highlight( info.comma_, info.shadow_, info.highlight_ );

   //
   // find any constant columns
   //
   int *const digitPixels = (int *)alloca(info.digitStrip_.width_*sizeof(int));
   unsigned short const *inRow = (unsigned short *)info.digitStrip_.pixData_ ;

   for( unsigned i = 0 ; i < info.digitStrip_.width_ ; i++ )
      digitPixels[i] = inRow[i];

   unsigned digitBitBytes = (info.digitStrip_.width_+7)/8 ;
   unsigned char *const digitBitmask = new unsigned char [digitBitBytes];
   memset( digitBitmask, 0xFF, sizeof( digitBitmask ) );

   std::set<unsigned short> colors ;
   
   for( unsigned y = 1 ; y < info.digitStrip_.height_ ; y++ )
   {
      inRow += info.digitStrip_.width_ ;
      for( unsigned x = 0 ; x < info.digitStrip_.width_ ; x++ )
      {
         unsigned const byte = x/8 ;
         unsigned char const mask = (1<<(x&7));
         unsigned short const pix = inRow[x];
         if( pix != digitPixels[x] )
            digitBitmask[byte] &= ~mask; // mismatch
         colors.insert(pix);
      }
   }
   
   unsigned matching = 0 ; 
   for( unsigned x = 0 ; x < info.digitStrip_.width_ ; x++ )
   {
      unsigned const byte = x/8 ;
      unsigned char const mask = (1<<(x&7));
      if( digitBitmask[byte] & mask )
         matching++ ;
   }
   printf( "%u constant columns\n", matching );
   printf( "%u colors\n", colors.size() );
   
   delete [] digitBitmask ;
   return true ;
}


struct digitInfo_t {
   digitInfo_t( image_t const       &digitStrip,
                rectangle_t const   &r,
                unsigned char const *shade,
                unsigned char const *highlight );
   ~digitInfo_t( void );

   void draw( fbDevice_t &fb,
              unsigned offs );
   
   image_t             const &digitStrip_ ;
   rectangle_t          const r_ ;
   image_t                    bg_ ;
   unsigned char const *const shade_ ;
   unsigned char const *const highlight_ ;
   unsigned                   lastOffset_ ;
};


digitInfo_t::digitInfo_t   
   ( image_t const       &digitStrip,
     rectangle_t const   &r,
     unsigned char const *shade,
     unsigned char const *highlight )
   : digitStrip_( digitStrip )
   , r_( r )
   , shade_( shade )
   , highlight_( highlight )
   , lastOffset_( -1UL )
{
   screenImageRect( getFB(), r_, bg_ );
}


digitInfo_t::~digitInfo_t( void )
{
//   showImage( getFB(), r_.xLeft_, r_.yTop_, bg_ );
}

void digitInfo_t::draw( 
   fbDevice_t &fb,
   unsigned    offs )
{
   if( offs != lastOffset_ )
   {
      lastOffset_ = offs ;
      drawDigit( fb, r_.
                 xLeft_, r_.yTop_,
                 r_.height_,
                 (unsigned short *)digitStrip_.pixData_,
                 digitStrip_.width_,
                 digitStrip_.height_,
                 offs,
                 shade_,
                 highlight_ );
   }
}

//
// returns the number of significant (non-zero) digits
//
static unsigned toDecimal( 
   unsigned value
,  unsigned numDigits
,  char    *output )
{
   output += numDigits ;
   *output-- = '\0' ;
   unsigned rval = 0 ;
   unsigned pos = 1 ;
   while( numDigits-- )
   {
      char const decimal = '0' + (value%10);
      *output-- = decimal ;
      if( '0' != decimal )
         rval = pos ;
      pos++ ;
      value /= 10 ;
   }
   
   return rval ;
}                

//
// Place and update a value at a specific point on the screen
//
struct valueInfo_t {
   valueInfo_t( valueGraphics_t const &vg,
                point_t         const &pt,
                unsigned               maxDigits );
   ~valueInfo_t( void );

   void setTarget( unsigned v );
   void draw( void );
   
   valueGraphics_t const &vg_ ;
   unsigned const     maxDigits_ ;
   unsigned const     x_ ;
   unsigned const     y_ ;
   bool const         needComma_ ;
   unsigned const     totalDigits_ ; // including dollar sign, decimal point and thousands separator
   unsigned const     xRight_ ;
   image_t            background_ ;
   digitInfo_t **const digits_ ;
   unsigned           target_ ;
   unsigned           value_ ;
   unsigned long      pixelTarget_ ;
   unsigned long      pixelValue_ ;
   unsigned long      velocity_ ;
   rectangle_t        rect_ ;
   unsigned           commaPos_ ;
   unsigned           dollarPos_ ;
   char               sTargetValue_[11];
   char               sValue_[11];
   unsigned           targetSD_ ; // significant digits
   unsigned           valueSD_ ;  //         "
   unsigned           frozen_ ;   // number of frozen leftmost digits
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
   , digits_( new digitInfo_t *[maxDigits] )
   , target_(0)
   , value_(0)
   , pixelTarget_(0)
   , pixelValue_(0)
   , velocity_(0)
   , rect_()
   , commaPos_( -1U )
   , dollarPos_( -1U )
   , targetSD_( 0 )
   , valueSD_( 0 )
   , frozen_( 0 )
{
   rect_.xLeft_  = pt.x ;
   rect_.yTop_   = pt.y ;
   rect_.width_  = xRight_ - pt.x ;
   rect_.height_ = vg.decimalPoint_.height_ ;
   
   fbDevice_t &fb = getFB();
   screenImageRect( fb, rect_, background_ );

   rectangle_t rdig ;
   
   rdig.xLeft_  = pt.x + vg.dollarSign_.width_ ;
   rdig.yTop_   = pt.y ;
   rdig.width_  = vg.digitStrip_.width_ ;
   rdig.height_ = vg.decimalPoint_.height_ ;

   unsigned decimalPos = 0 ;
   for( unsigned i = 0 ; i < maxDigits ; i++ )
   {
      if( maxDigits-2 == i ){
         decimalPos = rdig.xLeft_ ;
         rdig.xLeft_ += vg.decimalPoint_.width_ ;
      }
      else if( maxDigits-5 == i ){
         commaPos_ = rdig.xLeft_ ;
         rdig.xLeft_ += vg.comma_.width_ ;
      }
      digits_[i] = new digitInfo_t( vg.digitStrip_, rdig, vg.shadow_, vg.highlight_ );
      rdig.xLeft_ += vg.digitStrip_.width_ ;
   }

   showImage( fb, decimalPos, pt.y, vg.decimalPoint_ );
}

valueInfo_t::~valueInfo_t( void )
{
   for( unsigned i = 0 ; i < maxDigits_ ; i++ )
      delete digits_[i];
   delete [] digits_ ;
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

   targetSD_ = toDecimal( target_, maxDigits_, sTargetValue_ );
//   snprintf( sTargetValue_, sizeof(sTargetValue_), "%010u", target_ );
   frozen_   = 0 ;
   velocity_    = ( pixelTarget_ - pixelValue_ ) / TICKSTOTARGET ;
//   velocity_ = 60000 ;

}

void valueInfo_t::draw( void )
{
   if( pixelTarget_ != pixelValue_ )
   {
      fbDevice_t &fb = getFB();

      unsigned long pValue = pixelValue_ + velocity_ ;
      if( pValue > pixelTarget_ )
         pValue = pixelTarget_ ;

// printf( "%lu->%lu\n", pValue, pixelTarget_ );

      pixelValue_ = pValue ;
      value_ = pValue / rect_.height_ ;

      bool showDollar = false ;
      unsigned sigDigits = toDecimal( value_, maxDigits_, sValue_ );
      if( sigDigits != valueSD_ )
      {
         valueSD_ = sigDigits ;
         if( 5 < sigDigits ){
            showImage( fb, commaPos_, rect_.yTop_, vg_.comma_ );
         }
         showDollar = true ;
      } // increased number of significant digits

//      snprintf( sValue_, sizeof(sValue_), "%010u", value_ );

//      
      // re-count digits to freeze
      unsigned matching = 0 ;
      for( unsigned i = 0 ; i < maxDigits_ ; i++ ){
         if( sValue_[i] == sTargetValue_[i] )
            matching++ ;
         else
            break ;
      }

      unsigned dig = maxDigits_ ; 
      while( 0 < dig )
      {
         --dig ;
         char const target = sTargetValue_[dig];
         if( dig >= matching )
            digits_[dig]->draw(fb, pValue % vg_.digitStrip_.height_ );
         else if( dig >= frozen_ )
         {
/*
printf( "freeze on digit %d/'%c'/%u/%u\n", dig, target, matching, frozen_ );
printf( "%s:%s:%12lu:%10u:%u\n", sTargetValue_, sValue_, pixelValue_, value_, rect_.height_ );
*/
            digits_[dig]->draw(fb, (target-'0')*rect_.height_ );
            frozen_++ ;
            break ;
         }
         else {
            break ;
         }
         pValue /= 10 ;
         sigDigits-- ;
      }

      unsigned pos = digits_[dig]->r_.xLeft_ - vg_.dollarSign_.width_ ;
      if( showDollar ){
         dollarPos_ = pos ;
         showImage( fb, pos, rect_.yTop_, vg_.dollarSign_ );
      } // move dollar sign
         
/*
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
      }
*/
// velocity_ = 60000 ;         

/*
      if( matching != frozen_ ){
         printf( "%s:%s:%12lu:%10u:%u\n", sTargetValue_, sValue_, pixelValue_, value_, rect_.height_ );
         frozen_ = matching ;
         printf( "%u digits frozen\n", frozen_ );
      }
*/      
      velocity_ = ( pixelTarget_ - pixelValue_ ) / TICKSTOTARGET ;
      if( ( 0 == velocity_ )
          && 
          ( pixelTarget_ > pixelValue_ ) )
        velocity_ = 1 ;

/*      
      x -= vg_.dollarSign_.width_ ;
      if( dollarPos_ != x )
      {
         dollarPos_ = x ;
         showImage( fb, x, y_, vg_.dollarSign_ );
      }
*/
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
      if( !loadValueGraphics( "/tmp", vg ) )
         return -1 ;

      //
      // Initialize screen
      //
      fb.clear(0xFF, 0xFF, 0xFF);
      fb.render( 0, 0, 
            vg.background_.width_,
            vg.background_.height_,
            (unsigned short const *)vg.background_.pixData_ );

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
 
//   sleep(1);
           
         if( numSteady == numValues )
            break ;
/*
         if( increment_ )
         {
            increment_ = false ;
            unsigned which = iterations%numValues ;
            valueInfo_t &value = *( values[which] );
            if( iterations & 2 )
               value.setTarget( value.target_ + 50000 );
            else
               value.setTarget( value.target_ + 500000 );
//            printf( "value[%u] -> %u\n", which, value.target_ );
         }
*/
      }
      unsigned long vsyncEnd ;
      fb.syncCount(vsyncEnd);

      unsigned const elapsedS = time(0)-startTick ;
      printf( "%u iterations, %lu refreshes in %u seconds, (%u per s)\n",
         iterations, vsyncEnd-vsyncStart, elapsedS, iterations/elapsedS );
      printf( "%u ticks, %u drawing (%u mixing), %u flipping, %u other\n",
              totalTicks_, numDrawing_, numMixing_, numFlipping_, totalTicks_-numDrawing_-numFlipping_ );
              
      if( !doExit ){
         image_t screenImg ;
         screenImageRect( fb, values[0]->rect_, screenImg );

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
         
         for( unsigned i = 0 ; i < numValues ; i++ )
            delete values[i];
         delete [] values ;
      }
   }
   else
      fprintf( stderr, "Usage: %s x y [x y...]\n", argv[0] );
   return 0 ;
}
