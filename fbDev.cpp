/*
 * Module fbDev.cpp
 *
 * This module defines ...
 *
 *
 * Change History :
 *
 * $Log: fbDev.cpp,v $
 * Revision 1.17  2003-02-26 12:19:14  tkisky
 * -10.4 display shuffling bits
 *
 * Revision 1.16  2002/12/18 19:10:43  tkisky
 * -don't unscamble LCD data
 *
 * Revision 1.15  2002/12/11 04:04:48  ericn
 * -moved buttonize code from button to fbDev
 *
 * Revision 1.14  2002/12/07 21:01:06  ericn
 * -added antialias() method for text rendering
 *
 * Revision 1.13  2002/12/04 13:56:40  ericn
 * -changed line() to specify top/left of line instead of center
 *
 * Revision 1.12  2002/12/04 13:12:53  ericn
 * -added rect, line, box methods
 *
 * Revision 1.11  2002/11/23 16:13:11  ericn
 * -added render with alpha
 *
 * Revision 1.10  2002/11/22 21:31:43  tkisky
 * -Optimize render and use it in jsImage
 *
 * Revision 1.9  2002/11/22 15:13:18  ericn
 * -added method render()
 *
 * Revision 1.8  2002/11/22 10:58:55  tkisky
 * -Don't mess up display settings
 *
 * Revision 1.7  2002/11/21 23:03:00  tkisky
 * -Make it easy to turn off/change reordering
 *
 * Revision 1.6  2002/11/21 14:03:21  ericn
 * -removed display clear on instantiation
 *
 * Revision 1.5  2002/11/02 18:39:25  ericn
 * -added getRed(), getGreen(), getBlue() methods to descramble pins
 *
 * Revision 1.4  2002/10/31 02:06:28  ericn
 * -modified to allow run (sort of) without frame buffer
 *
 * Revision 1.3  2002/10/25 04:49:11  ericn
 * -removed debug statements
 *
 * Revision 1.2  2002/10/24 13:17:56  ericn
 * -modified to prevent pxafb complaints
 *
 * Revision 1.1  2002/10/15 05:01:47  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "fbDev.h"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <linux/fb.h>
#include <string.h>

static unsigned short rTable[32];
static unsigned short gTable[64];
static unsigned short bTable[32];


#if 0
// 0  1  2  3  4    5  6  7  8  9 10    11 12 13 14 15
//15 14  8  7  6   13 12 11  5  4  3    10  9  2  1  0
//15 14 13 10  9    8  4  3  2 12 11     7  6  5  1  0

#if 1
#define LCD_REORDER_BLUE  15,14,13,10, 9		//10.4 on new board
#define LCD_REORDER_GREEN  8, 4, 3, 2, 12, 11
#define LCD_REORDER_RED    7, 6, 5, 1, 0
#else
#define LCD_REORDER
#define LCD_REORDER_BLUE  15,14, 8, 7, 6		//5.7 on 1st board
#define LCD_REORDER_GREEN 13,12,11, 5, 4, 3
#define LCD_REORDER_RED   10, 9, 2, 1, 0
#endif

#else
#define LCD_REORDER_BLUE  0,1,2,3,4
#define LCD_REORDER_GREEN 5,6,7,8,9,10
#define LCD_REORDER_RED   11,12,13,14,15
#endif

#define M5Masks(a,b,c,d,e)   1<<a, 1<<b, 1<<c, 1<<d, 1<<e
#define M6Masks(a,b,c,d,e,f) 1<<a, 1<<b, 1<<c, 1<<d, 1<<e, 1<<f
//the macro indirection allows "a" to be evaluated as multiple parameters
#define Make5Masks(a) M5Masks(a)
#define Make6Masks(a) M6Masks(a)
static void InitBoundaryReordering(void)
{
	const char bMap[] = {LCD_REORDER_BLUE};
	const char gMap[] = {LCD_REORDER_GREEN};
	const char rMap[] = {LCD_REORDER_RED};
	int i,j;

   memset( rTable, 0, sizeof( rTable ) );
   memset( gTable, 0, sizeof( gTable ) );
   memset( bTable, 0, sizeof( bTable ) );

   for (j=0;j<5;j++)
	{
		int bm = 1<<j;
		unsigned short mask = 1<<rMap[j];
		for (i=0;i<32;i++) if (i & bm) rTable[i]|=mask;
	}
	for (j=0;j<6;j++)
	{
		int bm = 1<<j;
		unsigned short mask = 1<<gMap[j];
		for (i=0;i<64;i++) if (i & bm) gTable[i]|=mask;
	}
	for (j=0;j<5;j++)
	{
		int bm = 1<<j;
		unsigned short mask = 1<<bMap[j];
		for (i=0;i<32;i++) if (i & bm) bTable[i]|=mask;
	}
}

unsigned short fbDevice_t :: get16( unsigned char red, unsigned char green, unsigned char blue )
{
   return rTable[red>>3]
        | gTable[green>>2]
        | bTable[blue>>3];
}




unsigned char fbDevice_t :: getRed( unsigned short screenRGB )
{
#ifdef LCD_REORDER
   unsigned short const shortMasks[] = {Make5Masks(LCD_REORDER_RED)};
   unsigned char out = 0 ;
   unsigned char mask = 1 ;
   for( unsigned i = 0 ; i < 5 ; i++, mask <<= 1 )
      if( screenRGB & shortMasks[i] )
         out |= mask ;
   return out << 3 ;
#else
   return (screenRGB & (0x1f<<11)) >> (11-3);
#endif
}

unsigned char fbDevice_t :: getGreen( unsigned short screenRGB )
{
#ifdef LCD_REORDER
   unsigned short const shortMasks[] = {Make6Masks(LCD_REORDER_GREEN)};
   unsigned char out = 0 ;
   unsigned char mask = 1 ;
   for( unsigned i = 0 ; i < 6 ; i++, mask <<= 1 )
      if( screenRGB & shortMasks[i] )
         out |= mask ;
   return out << 2 ;
#else
   return (screenRGB & (0x3f<<5)) >> (5-2);
#endif
}

unsigned char fbDevice_t :: getBlue( unsigned short screenRGB )
{
#ifdef LCD_REORDER
   unsigned short const shortMasks[] = {Make5Masks(LCD_REORDER_BLUE)};
   unsigned char out = 0 ;
   unsigned char mask = 1 ;
   for( unsigned i = 0 ; i < 5 ; i++, mask <<= 1 )
      if( screenRGB & shortMasks[i] )
         out |= mask ;
   return out << 3 ;
#else
   return (screenRGB & 0x1f) << 3;
#endif
}

fbDevice_t :: fbDevice_t( char const *name )
   : fd_( open( name, O_RDWR ) ),
     mem_( 0 ),
     memSize_( 0 ),
     width_( 0 ),
     height_( 0 )
{
   InitBoundaryReordering();

   if( 0 <= fd_ )
   {
//      printf( "device %s opened\n", name );
      struct fb_fix_screeninfo fixed_info;
      int err = ioctl( fd_, FBIOGET_FSCREENINFO, &fixed_info);
      if( 0 == err )
      {
#if 0
         printf( "id %s\n", fixed_info.id );
         printf( "smem_start %lu\n", fixed_info.smem_start );
         printf( "smem_len   %lu\n", fixed_info.smem_len );
         printf( "type       %lu\n", fixed_info.type );
         printf( "type_aux   %lu\n", fixed_info.type_aux );
         printf( "visual     %lu\n", fixed_info.visual );
         printf( "xpan       %u\n", fixed_info.xpanstep );
         printf( "ypan       %u\n", fixed_info.ypanstep );
         printf( "ywrap      %u\n", fixed_info.ywrapstep );
         printf( "line_len   %u\n", fixed_info.line_length );
         printf( "mmio_start %lu\n", fixed_info.mmio_start );
         printf( "mmio_len   %lu\n", fixed_info.mmio_len );
         printf( "accel      %lu\n", fixed_info.accel );
#endif
         struct fb_var_screeninfo variable_info;

	      err = ioctl( fd_, FBIOGET_VSCREENINFO, &variable_info );
         if( 0 == err )
         {
#if 0
            printf( "xres              = %lu\n", variable_info.xres );			//  visible resolution
            printf( "yres              = %lu\n", variable_info.yres );
            printf( "xres_virtual      = %lu\n", variable_info.xres_virtual );		//  virtual resolution
            printf( "yres_virtual      = %lu\n", variable_info.yres_virtual );
            printf( "xoffset           = %lu\n", variable_info.xoffset );			//  offset from virtual to visible
            printf( "yoffset           = %lu\n", variable_info.yoffset );			//  resolution
            printf( "bits_per_pixel    = %lu\n", variable_info.bits_per_pixel );		//  guess what
            printf( "grayscale         = %lu\n", variable_info.grayscale );		//  != 0 Graylevels instead of colors

            printf( "red               = offs %lu, len %lu, msbr %lu\n",
                    variable_info.red.offset,
                    variable_info.red.length,
                    variable_info.red.msb_right );
            printf( "green             = offs %lu, len %lu, msbr %lu\n",
                    variable_info.green.offset,
                    variable_info.green.length,
                    variable_info.green.msb_right );
            printf( "blue              = offs %lu, len %lu, msbr %lu\n",
                    variable_info.blue.offset,
                    variable_info.blue.length,
                    variable_info.blue.msb_right );

            printf( "nonstd            = %lu\n", variable_info.nonstd );			//  != 0 Non standard pixel format
            printf( "activate          = %lu\n", variable_info.activate );			//  see FB_ACTIVATE_*
            printf( "height            = %lu\n", variable_info.height );			//  height of picture in mm
            printf( "width             = %lu\n", variable_info.width );			//  width of picture in mm
            printf( "accel_flags       = %lu\n", variable_info.accel_flags );		//  acceleration flags (hints)
            printf( "pixclock          = %lu\n", variable_info.pixclock );			//  pixel clock in ps (pico seconds)
            printf( "left_margin       = %lu\n", variable_info.left_margin );		//  time from sync to picture
            printf( "right_margin      = %lu\n", variable_info.right_margin );		//  time from picture to sync
            printf( "upper_margin      = %lu\n", variable_info.upper_margin );		//  time from sync to picture
            printf( "lower_margin      = %lu\n", variable_info.lower_margin );
            printf( "hsync_len         = %lu\n", variable_info.hsync_len );		//  length of horizontal sync
            printf( "vsync_len         = %lu\n", variable_info.vsync_len );		//  length of vertical sync
            printf( "sync              = %lu\n", variable_info.sync );			//  see FB_SYNC_*
            printf( "vmode             = %lu\n", variable_info.vmode );			//  see FB_VMODE_*
#endif
            if( ( 16 != variable_info.bits_per_pixel )
                ||
                ( 1 != variable_info.left_margin )
                ||
                ( 1 != variable_info.right_margin )
                ||
                ( 5 != variable_info.upper_margin )
                ||
                ( 5 != variable_info.lower_margin ) )
            {
               variable_info.left_margin     = 1 ;
               variable_info.right_margin    = 1 ;
               variable_info.upper_margin    = 5 ;
               variable_info.lower_margin    = 5 ;
               variable_info.bits_per_pixel  = 16 ;
               variable_info.red.offset      = 11 ;
               variable_info.red.length      = 5 ;
               variable_info.red.msb_right   = 0 ;
               variable_info.green.offset    = 5 ;
               variable_info.green.length    = 6 ;
               variable_info.green.msb_right = 0 ;
               variable_info.blue.offset     = 0 ;
               variable_info.blue.length     = 5 ;
               variable_info.blue.msb_right  = 0 ;
//             err = ioctl( fd_, FBIOPUT_VSCREENINFO, &variable_info );
//             if( err )  perror( "FBIOPUT_VSCREENINFO" );
            } // set to 16-bit

            if( 0 == err )
            {
               width_   = variable_info.xres ;
               height_  = variable_info.yres ;
               memSize_ = fixed_info.smem_len ;
               mem_ = mmap( 0, memSize_, PROT_WRITE|PROT_WRITE,
                            MAP_SHARED, fd_, 0 );
               if( MAP_FAILED != mem_ )
               {
//                  memset( mem_, 0, fixed_info.smem_len );
                  return ;
               }
               else
                  perror( "mmap fb" );
            } // had or changed to 16-bit color
         }
         else
            perror( "FBIOGET_VSCREENINFO" );
      }
      else
         perror( "FBIOGET_FSCREENINFO" );

      close( fd_ );
      fd_ = -1 ;
   }
   else
   {
      perror( name );
      printf( "simulating fbDevice" );
      width_ = 320 ;
      height_ = 240 ;
      memSize_ = width_ * height_ * sizeof( unsigned short );
      mem_ = new unsigned short[ width_ * height_ ];
   }
}

int min(int x,int y)
{
   return (x<y)? x : y;
}
void fbDevice_t :: render
   ( int xPos,				//placement on screen
     int yPos,
     int w,
     int h,	 			// width and height of image
     unsigned short const *pixels,
     int imagexPos,			//offset within image to start display
     int imageyPos,
     int imageDisplayWidth,		//portion of image to display
     int imageDisplayHeight
   )
{
   if (xPos<0)
   {
      imagexPos -= xPos;		//increase offset
      imageDisplayWidth += xPos;	//reduce width
      xPos = 0;
   }
   if (yPos<0)
   {
      imageyPos -= yPos;
      imageDisplayHeight += yPos;
      yPos = 0;
   }
   if ((imageDisplayWidth <=0)||(imageDisplayWidth >(w-imagexPos))) imageDisplayWidth = w-imagexPos;
   if ((imageDisplayHeight<=0)||(imageDisplayHeight>(h-imageyPos))) imageDisplayHeight = h-imageyPos;

   pixels += (w*imageyPos)+imagexPos;

   int minWidth = min(getWidth()-xPos,imageDisplayWidth) << 1;	//2 bytes/pixel
   int minHeight= min(getHeight()-yPos,imageDisplayHeight);
   if ((minWidth > 0) && (minHeight > 0))
   {
      do
      {
         unsigned short *pix = getRow( yPos++ ) + xPos;
         memcpy(pix,pixels,minWidth);
         pixels += w;
      } while (--minHeight);
   }
}

void fbDevice_t :: render
   ( unsigned short xPos, 
     unsigned short yPos,
     unsigned short w, 
     unsigned short h, // width and height of image
     unsigned short const *pixels,
     unsigned char const  *alpha )
{
   if( alpha )
   {
      if( ( 0 < w ) && ( 0 < h ) && ( xPos < getWidth() ) && ( yPos < getHeight() ) )
      {
         int const left = xPos ;
         for( unsigned y = 0 ; y < h ; y++, yPos++, alpha += w )
         {
            if( yPos < getHeight() )
            {
               unsigned short *pix = getRow( yPos ) + left ;
               xPos = left ;
               for( unsigned x = 0 ; x < w ; x++, xPos++ )
               {
                  if( xPos < getWidth() )
                  {
                     unsigned short const foreColor = pixels[y*w+x];
                     unsigned char const coverage = alpha[x];
                     if( 0xFF != coverage )
                     {
                        if( 0 != coverage )
                        {
                           unsigned short const bgColor = *pix ;
                           unsigned char const bgRed   = getRed( bgColor );
                           unsigned char const bgGreen = getGreen( bgColor );
                           unsigned char const bgBlue = getBlue( bgColor );

                           unsigned char const fgRed   = getRed( foreColor );
                           unsigned char const fgGreen = getGreen( foreColor );
                           unsigned char const fgBlue = getBlue( foreColor );
                           unsigned const alias = coverage + 1 ;
                           unsigned const notAlias = 256 - alias ;
                           
                           unsigned mixRed = ((alias*fgRed)+(notAlias*bgRed))/256 ;
                           unsigned mixGreen = ((alias*fgGreen)+(notAlias*bgGreen))/256 ;
                           unsigned mixBlue = ((alias*fgBlue)+(notAlias*bgBlue))/256 ;
                           *pix++ = get16( mixRed, mixGreen, mixBlue );
                        } // not entirely background, need to mix
                        else
                           pix++ ;
                     }
                     else
                        *pix++ = foreColor ;                     
                  }
                  else
                     break; // only going further off the screen
               }
            }
            else
               break; // only going further off the screen
         }
      }
   }
   else
      render( xPos, yPos, w, h, pixels );
}

#define swap( v1, v2 ) { unsigned short t = v1 ; v1 = v2 ; v2 = t ; }

void fbDevice_t :: rect
   ( unsigned short x1, unsigned short y1,
     unsigned short x2, unsigned short y2,
     unsigned char red, unsigned char green, unsigned char blue )
{
   unsigned short const rgb = get16( red, green, blue );

   if( x1 > x2 )
      swap( x1, x2 );
   if( y1 > y2 )
      swap( y1, y2 );
   
   if( ( x1 < getWidth() ) && ( y1 < getHeight() ) )
   {
      if( x2 >= getWidth() )
         x2 = getWidth() - 1 ;
      unsigned short const w = x2 - x1 + 1 ;

      if( y2 >= getHeight() )
         y2 = getHeight() - 1 ;

      unsigned short *row = getRow( y1 ) + x1 ;
      unsigned short *endRow = row + ( y2 - y1 ) * getWidth();
      while( row <= endRow )
      {
         unsigned short *next = row ;
         unsigned short * const endLine = next + w ;
         while( next < endLine )
            *next++ = rgb ;
         row += getWidth();
      }
   } // something is visible
}

void fbDevice_t :: line
   ( unsigned short x1, unsigned short y1,
     unsigned short x2, unsigned short y2,
     unsigned char penWidth,
     unsigned char red, unsigned char green, unsigned char blue )
{
   unsigned short const rgb = get16( red, green, blue );
   if( y1 == y2 )
   {
      if( y1 < getHeight()  )
         rect( x1, y1, x2, y1+penWidth-1, red, green, blue );
   } // horizontal
   else if( x1 == x2 )
   {
      if( x1 < getWidth() )
         rect( x1, y1, x1+penWidth-1, y2, red, green, blue );
   } // vertical
   else
      fprintf( stderr, "diagonal lines %u/%u/%u/%u not (yet) supported\n", x1, y1, x2, y2 );
}


void fbDevice_t :: box
   ( unsigned short x1, unsigned short y1,
     unsigned short x2, unsigned short y2,
     unsigned char penWidth,
     unsigned char red, unsigned char green, unsigned char blue )
{
   unsigned short const rgb = get16( red, green, blue );

   if( x1 > x2 )
      swap( x1, x2 );
   if( y1 > y2 )
      swap( y1, y2 );

   if( ( y1 < getHeight() )
       &&
       ( x1 < getWidth() ) )
   {
      // draw vertical lines
      line( x1, y1, x1, y2, penWidth, red, green, blue );
      x2 = x2 - penWidth + 1 ;
      line( x2, y1, x2, y2, penWidth, red, green, blue );

      // horizontal lines
      line( x1, y1, x2, y1, penWidth, red, green, blue );
      y2 = y2 - penWidth + 1 ;
      line( x1, y2, x2, y2, penWidth, red, green, blue );
   } // something is visible
}

void fbDevice_t :: antialias
   ( unsigned char const *bmp,
     unsigned short       bmpWidth,  // row stride
     unsigned short       bmpHeight, // num rows in bmp
     unsigned short       xLeft,     // display coordinates: clip to this rectangle
     unsigned short       yTop,
     unsigned short       xRight,
     unsigned short       yBottom,
     unsigned char red, unsigned char green, unsigned char blue )
{
   unsigned height = yBottom-yTop+1 ;
   if( height > bmpHeight )
      height = bmpHeight ;
   unsigned width = xRight-xLeft+1 ;
   if( width > bmpWidth )
      width = bmpWidth ; 

   unsigned short const fullColor = get16( red, green, blue );

   for( unsigned row = 0 ; row < height ; row++ )
   {
      if( yTop < getHeight() )
      {
         unsigned char const *alphaCol = bmp + (row*bmpWidth);
         unsigned short      *screenPix = getRow( yTop++ ) + xLeft ;

         unsigned short screenCol = xLeft ;
         for( unsigned col = 0 ; col < width ; col++, screenCol++, screenPix++, alphaCol++ )
         {
            if( screenCol < getWidth() )
            {
               unsigned char const coverage = *alphaCol ;
               if( 0xFF != coverage )
               {
                  if( 0 != coverage )
                  {
                     unsigned short const bgColor = *screenPix ;
                     unsigned char const bgRed   = getRed( bgColor );
                     unsigned char const bgGreen = getGreen( bgColor );
                     unsigned char const bgBlue  = getBlue( bgColor );

                     unsigned const alias = coverage + 1 ;
                     unsigned const notAlias = 256 - alias ;
                     
                     unsigned mixRed = ((alias*red)+(notAlias*bgRed))/256 ;
                     unsigned mixGreen = ((alias*red)+(notAlias*bgGreen))/256 ;
                     unsigned mixBlue = ((alias*red)+(notAlias*bgBlue))/256 ;
                     *screenPix = get16( mixRed, mixGreen, mixBlue );
                  } // not entirely background, need to mix
               } // not entirely foreground, need to mix
               else
                  *screenPix = fullColor ;                     
            }
            else
               break;
         }
      }
      else
         break;
   }
}

void fbDevice_t :: buttonize
   ( bool                 pressed,
     unsigned char        borderWidth,
     unsigned short       xLeft,
     unsigned short       yTop,
     unsigned short       xRight,
     unsigned short       yBottom,
     unsigned char red, unsigned char green, unsigned char blue )
{
   unsigned char const bgColors[3] = { 
      red, green, blue
   };

   unsigned char darkIncr[3] = {
      bgColors[0]/borderWidth,
      bgColors[1]/borderWidth,
      bgColors[2]/borderWidth
   };

   unsigned char liteDecr[3] = {
      (256-bgColors[0])/borderWidth,
      (256-bgColors[1])/borderWidth,
      (256-bgColors[2])/borderWidth
   };
   
   unsigned char lite[3] = {
      0xFF, 0xFF, 0xFF
   };

   unsigned char dark[3] = {
      0, 0, 0
   };

   unsigned short b[4] = { xLeft, yTop, xRight, yBottom };

   unsigned char const *const topLeft     = pressed ? dark : lite ;
   unsigned char const *const bottomRight = pressed ? lite : dark ;

   //
   // draw something that looks like a button
   //
   for( unsigned char i = 0 ; i < borderWidth ; i++ )
   {
      line( b[0], b[1], b[2], b[1], 1, topLeft[0],topLeft[1],topLeft[2] );
      line( b[0], b[1], b[0], b[3], 1, topLeft[0],topLeft[1],topLeft[2] );
      line( b[0], b[3], b[2], b[3], 1, bottomRight[0],bottomRight[1],bottomRight[2] );
      line( b[2], b[1], b[2], b[3], 1, bottomRight[0],bottomRight[1],bottomRight[2] );

      b[0]++ ; b[1]++ ; b[2]-- ; b[3]-- ;

      for( unsigned j = 0 ; j < 3 ; j++ )
      {
         lite[j] -= liteDecr[j];
         dark[j] += darkIncr[j];
      } // change shades
   } // draw bounding box

   rect( b[0], b[1], b[2], b[3], bgColors[0], bgColors[1], bgColors[2] );
}

fbDevice_t :: ~fbDevice_t( void )
{
   if( 0 <= fd_ )
   {
      munmap( mem_, memSize_ );
      close( fd_ );
   }
   else if( 0 != mem_ )
   {
      delete [] (unsigned short *)mem_ ;
   }
}

static fbDevice_t *dev_ = 0 ;

fbDevice_t &getFB( char const *devName )
{
   if( 0 == dev_ )
      dev_ = new fbDevice_t( devName );
   return *dev_ ;
}

#ifdef __MODULETEST__
#include <stdlib.h>

int main( int argc, char const * const argv[] )
{
   if( 1 < argc )
   {
      fbDevice_t &fb = getFB( argv[1] );
      if( fb.isOpen() )
      {
#ifdef ALLCOLORS
         unsigned short * const memStart = (unsigned short *)fb.getMem();
         unsigned short * const memEnd = (unsigned short *)( (char *)fb.getMem() + fb.getMemSize() );
   
         for( unsigned i = 0 ; i < 65536 ; i++ )
         {
            for( unsigned short *p = memStart ; p < memEnd ; p++ )
               *p = (unsigned short)i ;
         }
#else

         unsigned short *mem = (unsigned short *)fb.getMem();
         if( 2 == argc )
         {
            unsigned red = 0 ; 
            unsigned green = 0 ;
            unsigned blue = 0 ;
            
            for( unsigned row = 0 ; row < fb.getHeight() ; row++ )
            {
               red = green = blue = 0 ;
               for( unsigned col = 0 ; col < fb.getWidth(); col++ )
               {
                  red = ( red+1 ) & 31 ;
                  green = ( green+2 ) & 63 ;
                  blue  = ( blue+1 ) & 31 ;
                  unsigned short color = rTable[red] | gTable[green] | bTable[blue];
   
                  *mem++ = color ; // ( color >> 8 ) | ( ( color & 0xFF ) << 8 );
               }
            }
         } // shades of gray white
         else
         {
            char *endptr ;
            unsigned long colorVal = strtoul( argv[2], &endptr, 0 );
            if( '\0' == *endptr )
            {
               printf( "setting video %lu/0x%lX\n", colorVal, colorVal );
               for( unsigned row = 0 ; row < fb.getHeight() ; row++ )
               {
                  for( unsigned col = 0 ; col < fb.getWidth(); col++ )
                  {
                     *mem++ = (unsigned short)colorVal ;
                  }
               }
            } // valid number
            else
            {
               printf( "unsupported operation\n" );
            }
         }

#endif
      }
      else
         fprintf( stderr, "Error opening %s\n", argv[1] );
   }
   else
      fprintf( stderr, "Usage fbDev device [R G B]\n" );

   return 0 ;
}

#endif
