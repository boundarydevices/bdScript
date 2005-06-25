#include <pixman.h>
#include "fbDev.h"
#include <stdio.h>
#include "hexDump.h"
#include <math.h>
#include <string.h>

static pixman_color_t const whiteOpaque = {
    red:   0xFF00,
    green: 0xFF00,
    blue:  0xFF00,
    alpha: 0xFFFF,
};

static pixman_color_t const blackOpaque = {
    red:   0x0000,
    green: 0x0000,
    blue:  0x0000,
    alpha: 0xFFFF,
};

static pixman_color_t const redOpaque = {
    red:   0xFF00,
    green: 0x0000,
    blue:  0x0000,
    alpha: 0xFFFF,
};

static pixman_color_t const greenOpaque = {
    red:   0x0000,
    green: 0xFF00,
    blue:  0x0000,
    alpha: 0xFFFF,
};

static pixman_color_t const blueOpaque = {
    red:   0x0000,
    green: 0x0000,
    blue:  0xFF00,
    alpha: 0xFFFF,
};

static pixman_bits_t *getRow(pixman_image_t *img, unsigned row )
{
   if( row < pixman_image_get_height(img))
   {
      return (pixman_bits_t *)
             (((char const *)pixman_image_get_data(img))
              +(pixman_image_get_stride(img)*row));
   }
   else
      return 0 ;
}

inline unsigned char alphaOf( pixman_bits_t rgba )
{
   return (rgba>>24);
}

inline unsigned char redOf( pixman_bits_t rgba )
{
   return (rgba>>16);
}

inline unsigned char greenOf( pixman_bits_t rgba )
{
   return (rgba>>8);
}

inline unsigned char blueOf( pixman_bits_t rgba )
{
   return (rgba);
}

#define doubleToFixed(f)    ((pixman_fixed16_16_t) ((f) * 65536))

/*
double angle = M_PI / 180 * 30; // 30 degrees
double sina = std::sin( angle );
double cosa = std::cos( angle );

// Rotation matrix
XTransform xform = {{
	{ XDoubleToFixed(  cosa ), XDoubleToFixed( sina ), XDoubleToFixed( 0 ) },
	{ XDoubleToFixed( -sina ), XDoubleToFixed( cosa ), XDoubleToFixed( 0 ) },
	{ XDoubleToFixed(     0 ), XDoubleToFixed(    0 ), XDoubleToFixed( 1 ) }
}};
*/

static void rotationMatrix( unsigned angle, pixman_transform_t &transform )
{
   double rad = M_PI / 180 * angle ; // angle in degrees
   double sina = std::sin( rad );
   double cosa = std::cos( rad );

   memset( &transform, 0, sizeof(transform));
   transform.matrix[0][0] = doubleToFixed( cosa );
   transform.matrix[0][1] = doubleToFixed( sina );
   transform.matrix[1][0] = doubleToFixed( -sina );
   transform.matrix[1][1] = doubleToFixed( sina );
   transform.matrix[2][2] = 1;
}

static void toFB( pixman_image_t *img,
                  fbDevice_t     &fb )
{
   unsigned const height = pixman_image_get_height(img);
   for( unsigned row = 0 ; row < height ; row++ )
   {
      pixman_bits_t *imgRow = getRow( img, row );
      for( unsigned col = 0 ; col < fb.getWidth(); col++ )
      {
         pixman_bits_t rgba = *imgRow++ ;
         unsigned const alpha = alphaOf(rgba);
         unsigned const red = redOf(rgba);
         unsigned const green = greenOf(rgba);
         unsigned const blue = blueOf(rgba);
         if( (row < 5) && (col < 5))
            printf("%u:%u == %02x/%02x/%02x/%02x\n", col, row, alpha, red, green, blue );
         fb.setPixel( col, row, fb.get16( red, green, blue ) );
      }
   }
}

int main( void )
{
   printf( "Hello, pixman\n" );

   pixman_format_t *fmt = pixman_format_create(PIXMAN_FORMAT_NAME_RGB24);

   if( fmt )
   {
      printf( "format created\n" );
      fbDevice_t &fb = getFB();
      
      pixman_image_t *img = pixman_image_create(fmt, fb.getWidth(), fb.getHeight() );
      if( img )
      {
         printf( "image created: bits == %p\n", pixman_image_get_data(img) );

         printf( "image stride= %d\n", pixman_image_get_stride(img));

         pixman_fill_rectangle( PIXMAN_OPERATOR_OVER,
                                img,
                                &whiteOpaque,
                                0, 0, 
                                fb.getWidth(), fb.getHeight() );

/* red */ pixman_fill_rectangle( PIXMAN_OPERATOR_OVER, img, &redOpaque, 
                                 10, 10,
//                                 (fb.getWidth()/2)-5, 
//                                 (fb.getHeight()/2)-5, 
                                 10, 10 );

         pixman_transform_t transform ;
         rotationMatrix( 45, transform );
         pixman_image_set_transform(img, &transform );

/* green */ pixman_fill_rectangle( PIXMAN_OPERATOR_OVER, img, &greenOpaque, 
                                   10, 10,
//                                 (fb.getWidth()/2)-5, 
//                                 (fb.getHeight()/2)-5, 
                                   10, 10 );
         
         hexDumper_t dumpImg( pixman_image_get_data(img), 64 );
         while( dumpImg.nextLine() )
            printf( "%s\n", dumpImg.getLine() );

         toFB( img, fb );

         printf( "image destroyed\n" );
      }
      else
         printf( "Error creating image\n" );

      pixman_format_destroy(fmt);

      printf( "format destroyed\n" );
   }
   else
      printf( "Error creating format\n" );

   return 0 ;
}
