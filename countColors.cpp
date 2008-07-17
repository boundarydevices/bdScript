/*
 * countColors.cpp
 * 
 * Program to count Y, U, and V values in one or more
 * YUV files.
 *
 * Prints output of each file as well as totals for all
 * files.
 *
 */

#include <stdio.h>
#include "memFile.h"
#include <zlib.h>
#include <string.h>

class colorCounter_t {
public:
   colorCounter_t( void );
   ~colorCounter_t( void );

   colorCounter_t &operator += (colorCounter_t const &);

   inline void addY( unsigned char Y );
   inline void addU( unsigned char U ); 
   inline void addV( unsigned char V ); 
   unsigned numY( void ) const { return numY_ ; }
   unsigned numU( void ) const { return numU_ ; }
   unsigned numV( void ) const { return numV_ ; }
   unsigned yCount( unsigned char Y ){ return y_[Y]; }
   unsigned uCount( unsigned char U ){ return u_[U]; }
   unsigned vCount( unsigned char V ){ return v_[V]; }
   unsigned minY(void) const { return minY_ ; }
   unsigned maxY(void) const { return maxY_ ; }
   unsigned minU(void) const { return minU_ ; }
   unsigned maxU(void) const { return maxU_ ; }
   unsigned minV(void) const { return minV_ ; }
   unsigned maxV(void) const { return maxV_ ; }
private:
   colorCounter_t( colorCounter_t const & ); // no copies
   unsigned *y_ ;
   unsigned *u_ ; 
   unsigned *v_ ;
   unsigned char minY_ ;
   unsigned char maxY_ ;
   unsigned numY_ ;
   unsigned char minU_ ;
   unsigned char maxU_ ;
   unsigned numU_ ;
   unsigned char minV_ ;
   unsigned char maxV_ ;
   unsigned numV_ ;
};

colorCounter_t::colorCounter_t( void )
   : y_( new unsigned [3*256] )
   , u_( y_ + 256 )
   , v_( u_ + 256 )
   , minY_( 255 )
   , maxY_( 0 )
   , numY_( 0 )
   , minU_( 255 )
   , maxU_( 0 )
   , numU_( 0 )
   , minV_( 255 )
   , maxV_( 0 )
   , numV_( 0 )
{
   memset( y_, 0, 3*256*sizeof(y_[0]) );
}

colorCounter_t::~colorCounter_t( void )
{
   delete [] y_ ;
}

colorCounter_t &colorCounter_t::operator += (colorCounter_t const &rhs)
{
   numY_ = numU_ = numV_ = 0 ;
   minY_ = 
   minU_ = 
   minV_ = 255 ;
   maxY_ = 
   maxU_ = 
   maxV_ = 0 ;
   for( unsigned i = 0 ; i < 256 ; i++ ){
      y_[i] += rhs.y_[i];
      if( y_[i] ){
         numY_++ ;
         maxY_ = i ;
         if( 255 == minY_ )
            minY_ = i ;
      }
      u_[i] += rhs.u_[i];
      if( u_[i] ){
         numU_++ ;
         maxU_ = i ;
         if( 255 == minU_ )
            minU_ = i ;
      }
      v_[i] += rhs.v_[i];
      if( v_[i] ){
         numV_++ ;
         numV_++ ;
         maxV_ = i ;
         if( 255 == minV_ )
            minV_ = i ;
      }
   }
   return *this ;
}

void colorCounter_t::addY( unsigned char Y )
{
   unsigned count = y_[Y];
   if( 0 == count ){
      if( Y < minY_ )
         minY_ = Y ;
      if( Y > maxY_ )
         maxY_ = Y ;
      numY_++ ;
   }
   y_[Y] = count + 1 ;
}

void colorCounter_t::addU( unsigned char U )
{
   unsigned count = u_[U];
   if( 0 == count ){
      if( U < minU_ )
         minU_ = U ;
      if( U > maxU_ )
         maxU_ = U ;
      numU_++ ;
   }
   u_[U] = count + 1 ;
}

void colorCounter_t::addV( unsigned char V )
{
   unsigned count = v_[V];
   if( 0 == count ){
      if( V < minV_ )
         minV_ = V ;
      if( V > maxV_ )
         maxV_ = V ;
      numV_++ ;
   }
   v_[V] = count + 1 ;
}

int main( int argc, char const * const argv[] )
{
   colorCounter_t global ;
   for( unsigned arg = 1 ; arg < argc ; arg++ ){
      memFile_t fIn( argv[arg] );
      if( !fIn.worked() ){
         perror( argv[arg] );
         continue;
      }
      unsigned numPix = fIn.getLength()*2 / 3 ;
      printf( "%s: %u pixels\n", argv[arg], numPix );
      colorCounter_t local ;
      unsigned char const *next = (unsigned char const *)fIn.getData();
      for( unsigned i = 0 ; i < numPix ; i++ ){
         local.addY( *next++ );
      }
      numPix /= 4 ;
      for( unsigned i = 0 ; i < numPix ; i++ ){
         local.addU( *next++ );
      }
      for( unsigned i = 0 ; i < numPix ; i++ ){
         local.addV( *next++ );
      }
      global += local ;
      printf( "%s: %u, %u, %u\n", argv[arg], local.numY(), local.numU(), local.numV() );
   }
   printf( "global: %u, %u, %u\n", global.numY(), global.numU(), global.numV() );
   for( unsigned i = 0 ; i < 256 ; i++ ){
//      printf( "Y[%u] == %u\n", i, global.yCount(i) );
   }
   printf( "yRange: %u..%u (%u)\n", global.minY(), global.maxY(), global.maxY()-global.minY() );
   for( unsigned i = 0 ; i < 256 ; i++ ){
//      printf( "U[%u] == %u\n", i, global.uCount(i) );
   }
   printf( "uRange: %u..%u (%u)\n", global.minU(), global.maxU(), global.maxU()-global.minU() );
   for( unsigned i = 0 ; i < 256 ; i++ ){
//      printf( "V[%u] == %u\n", i, global.vCount(i) );
   }
   printf( "vRange: %u..%u (%u)\n", global.minV(), global.maxV(), global.maxV()-global.minV() );
   return 0 ;
}
