/*
 * Module fbDev.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: fbDev.cpp,v $
 * Revision 1.3  2002-10-25 04:49:11  ericn
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

static void InitBoundaryReordering(void)
{
	const char rMap[] = {10,9,2,1,0};
	const char gMap[] = {13,12,11,5,4,3};
	const char bMap[] = {15,14,8,7,6};
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
/*
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
*/
         struct fb_var_screeninfo variable_info;

	      err = ioctl( fd_, FBIOGET_VSCREENINFO, &variable_info );
         if( 0 == err )
         {
/*
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
*/
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
               err = ioctl( fd_, FBIOPUT_VSCREENINFO, &variable_info );
               if( err )
                  perror( "FBIOPUT_VSCREENINFO" );
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
                  memset( mem_, 0, fixed_info.smem_len );
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
      perror( name );
}

fbDevice_t :: ~fbDevice_t( void )
{
   if( 0 <= fd_ )
      close( fd_ );
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

