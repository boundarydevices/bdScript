/*
 * Module pxaYUV.cpp
 *
 * This module defines the methods of the pxaYUV_t class as 
 * declared in pxaYUV.h
 *
 *
 * Change History : 
 *
 * $Log$
 *
 * Copyright Boundary Devices, Inc. 2009
 */

#include "pxaYUV.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

#define PXA27X_Y_CLASS "fb_y"
#define PXA27X_U_CLASS "fb_u"
#define PXA27X_V_CLASS "fb_v"

#define BASE_MAGIC 0xBD
#define PXA27X_YUV_SET_DIMENSIONS  _IOWR(BASE_MAGIC, 0x03, struct pxa27x_overlay_t)

#define FOR_RGB 0
#define FOR_PACKED_YUV444 1
#define FOR_PLANAR_YUV444 2
#define FOR_PLANAR_YUV422 3
#define FOR_PLANAR_YUV420 4

struct pxa27x_overlay_t {
	unsigned for_type;
	unsigned offset_x;	/* relative to the base plane */
	unsigned offset_y;
	unsigned width;
	unsigned height;
};

pxaYUV_t::pxaYUV_t( unsigned x, unsigned y, unsigned w, unsigned h )
   : fd_y(-1)
   , fd_u(-1)
   , fd_v(-1)
   , map_y(MAP_FAILED)
   , map_u(MAP_FAILED)
   , map_v(MAP_FAILED)
   , width(w)
   , height(h)
   , planar_y_bytes(((w*h) + 15) & ~15) 
   , planar_uv_bytes(w*h/2)
{
   fd_y = open( "/dev/" PXA27X_Y_CLASS, O_RDWR );
   if(fd_y < 0)
      return ;
   printf( "opened %s\n", "/dev/" PXA27X_Y_CLASS );
   fd_u = open( "/dev/" PXA27X_U_CLASS, O_RDWR );
   if(fd_u < 0)
      goto bail ;
   printf( "opened %s\n", "/dev/" PXA27X_U_CLASS );
   fd_v = open( "/dev/" PXA27X_V_CLASS, O_RDWR );
   if(fd_v < 0)
      goto bail ;
   printf( "opened %s\n", "/dev/" PXA27X_V_CLASS );

   pxa27x_overlay_t pi ;
   pi.for_type = FOR_PLANAR_YUV420;
   pi.offset_x = x;
   pi.offset_y = y;
   pi.width  = w;
   pi.height = h;

   if(0 != ioctl(fd_y, PXA27X_YUV_SET_DIMENSIONS, &pi))
     goto bail ;

   printf( "set dimensions to %ux%u @%u:%u\n", w, h, x, y );
   map_y = mmap(NULL, planar_y_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd_y, 0);
   if(MAP_FAILED==map_y)
      goto bail ;
   printf( "mapped %s\n", "/dev/" PXA27X_Y_CLASS );
   map_u = mmap(NULL, planar_uv_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd_u, 0);
   if(MAP_FAILED==map_u)
      goto bail ;
   printf( "mapped %s\n", "/dev/" PXA27X_U_CLASS );
   map_v = mmap(NULL, planar_uv_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd_v, 0);
   if(MAP_FAILED==map_v)
      goto bail ;
   printf( "mapped %s\n", "/dev/" PXA27X_V_CLASS );

   return ;

bail:
   if(map_y){
      munmap(map_y,planar_y_bytes);
      map_y = MAP_FAILED ;
   }
   close(fd_y);
   fd_y = -1 ;       // force !isOpen. destructor will handle the rest
}

pxaYUV_t::~pxaYUV_t( void )
{
   if(MAP_FAILED != map_y)
      munmap(map_y,planar_y_bytes);
   if(MAP_FAILED != map_u)
      munmap(map_u,planar_uv_bytes);
   if(MAP_FAILED != map_v)
      munmap(map_v,planar_uv_bytes);
   if( 0 <= fd_y )
      close(fd_y);
   if( 0 <= fd_u )
      close(fd_u);
   if( 0 <= fd_v )
      close(fd_v);
}

void pxaYUV_t::writeInterleaved( unsigned char const *data )
{
   if(!isOpen())
      return ;
   unsigned char *yOut = (unsigned char *)map_y ;
   unsigned char *uOut = (unsigned char *)map_u ;
   unsigned char *vOut = (unsigned char *)map_v ;
   for( unsigned row = 0 ; row < height ; row++){
      for( unsigned col = 0 ; col < width ; col++ ){
         *yOut++ = *data++ ;
         unsigned char c = 
            *data++ ;
         if( row & 1 ){
            if(0 == (col&1))
               *uOut++ = c ;
            else
               *vOut++ = c ;
         }
      }
   }
}

#ifdef STANDALONE_PXAYUV

#include <stdlib.h>

int main( int argc, char const * const argv[] )
{
   if( 5 == argc ){
      unsigned x = strtoul(argv[1],0,0);
      unsigned y = strtoul(argv[2],0,0);
      unsigned w = strtoul(argv[3],0,0);
      unsigned h = strtoul(argv[4],0,0);
      pxaYUV_t fb(x,y,w,h);
      if(fb.isOpen()) {
         printf( "opened\n" );
         char inBuf[80];
         fgets(inBuf,sizeof(inBuf),stdin);
      }
      else
         fprintf( stderr, "Error opening or mapping devs\n" );
   }
   else {
      fprintf( stderr, "Usage: %s x y w h\n", argv[0] );
   }
}
#endif
