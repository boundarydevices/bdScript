#ifndef __FBDEV_H__
#define __FBDEV_H__ "$Id: fbDev.h,v 1.30 2009-06-03 21:26:00 tkisky Exp $"

/*
 * fbDev.h
 *
 * This header file declares the fbDevice_t class, 
 * which sets up the PXA-250 frame buffer for 16-bit
 * color, and memory maps the display RAM.
 *
 * Note that the c
 *
 * Change History : 
 *
 * $Log: fbDev.h,v $
 * Revision 1.30  2009-06-03 21:26:00  tkisky
 * -add screen.release function
 *
 * Revision 1.29  2009-05-14 16:26:18  ericn
 * [multi-display SM-501] Add frame-buffer offsets
 *
 * Revision 1.28  2008-10-16 00:09:38  ericn
 * [getFB()] Allow fbDev.cpp to read environment variable FBDEV to determine default FB
 *
 * Revision 1.27  2008-09-22 18:36:49  ericn
 * [davinci] Add some coexistence stuff for davinci_code
 *
 * Revision 1.26  2007-08-08 17:11:54  ericn
 * -[sm501] /dev/fb0 not /dev/fb/0
 *
 * Revision 1.25  2006/12/01 19:57:21  tkisky
 * -make odometer compile
 *
 * Revision 1.24  2006/09/24 16:20:24  ericn
 * -add render with transparent color
 *
 * Revision 1.23  2006/08/16 14:49:22  ericn
 * -no double-buffering
 *
 * Revision 1.22  2006/06/14 13:51:17  ericn
 * -return syncCount in waitSync
 *
 * Revision 1.21  2006/06/06 03:06:45  ericn
 * -preliminary double-buffering
 *
 * Revision 1.20  2006/03/28 04:24:46  ericn
 * -make getMem() public (I know, I know)
 *
 * Revision 1.19  2005/11/06 16:01:58  ericn
 * -KERNEL_FB, not CONFIG_BD2003
 *
 * Revision 1.18  2005/11/05 20:23:16  ericn
 * -change default fbdev name
 *
 * Revision 1.17  2004/11/16 07:31:11  tkisky
 * -add ConvertRgb24LineTo16
 *
 * Revision 1.16  2004/09/25 21:48:44  ericn
 * -added render(bitmap) method
 *
 * Revision 1.15  2004/05/08 14:23:02  ericn
 * -added drawing primitives to image object
 *
 * Revision 1.14  2004/05/05 03:18:59  ericn
 * -add render and antialias into image
 *
 * Revision 1.13  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.12  2003/11/24 19:42:05  ericn
 * -polling touch screen
 *
 * Revision 1.11  2003/03/12 02:57:14  ericn
 * -added blend() method
 *
 * Revision 1.10  2002/12/11 04:04:48  ericn
 * -moved buttonize code from button to fbDev
 *
 * Revision 1.9  2002/12/07 21:01:19  ericn
 * -added antialias() method for text rendering
 *
 * Revision 1.8  2002/12/04 13:56:37  ericn
 * -changed line() to specify top/left of line instead of center
 *
 * Revision 1.7  2002/12/04 13:12:55  ericn
 * -added rect, line, box methods
 *
 * Revision 1.6  2002/11/23 16:09:14  ericn
 * -added render with alpha
 *
 * Revision 1.5  2002/11/22 21:31:43  tkisky
 * -Optimize render and use it in jsImage
 *
 * Revision 1.4  2002/11/22 15:08:41  ericn
 * -added method render()
 *
 * Revision 1.3  2002/11/02 18:39:16  ericn
 * -added getRed(), getGreen(), getBlue() methods to descramble pins
 *
 * Revision 1.2  2002/10/31 02:06:23  ericn
 * -modified to allow run (sort of) without frame buffer
 *
 * Revision 1.1  2002/10/15 05:01:47  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#ifndef __BITMAP_H__
#include "bitmap.h"
#endif 

#include "config.h"

struct point_t {
   int x ;
   int y ;
};

struct rectangle_t {
   unsigned xLeft_ ;
   unsigned yTop_ ;
   unsigned width_ ;
   unsigned height_ ;
};

inline rectangle_t makeRect( 
   unsigned __x,
   unsigned __y,
   unsigned __w,
   unsigned __h )
{
   rectangle_t r ;
   r.xLeft_  = __x;
   r.yTop_   = __y;
   r.width_  = __w;
   r.height_ = __h;
   return r ;
}

class fbDevice_t {
public:
   bool isOpen( void ) const { return 0 != mem_ ; }

//#ifdef KERNEL_FB_SM501
   bool syncCount( unsigned long &value ) const ;

   void waitSync( unsigned long &syncCount ) const ;
//#endif

   unsigned short getWidth( void ) const { return width_ ; }
   unsigned short getHeight( void ) const { return height_ ; }
   unsigned short getStride( void ) const { return stride; }

   unsigned long getMemSize( void ) const { return memSize_ ; }

   unsigned short getPixel( unsigned x, unsigned y );
   void           setPixel( unsigned x, unsigned y, unsigned short rgb );

   inline static unsigned short get16( unsigned long rgb );

   static void ConvertRgb24LineTo16(unsigned short* fbMem, unsigned char const *video,int cnt);
   static unsigned short get16( unsigned char red, unsigned char green, unsigned char blue );

   //
   // un-mix the screen bits and return 8-bit values for each of red, green, blue
   //
   static unsigned char getRed( unsigned short screenRGB );
   static unsigned char getGreen( unsigned short screenRGB );
   static unsigned char getBlue( unsigned short screenRGB );
   static void releaseFB();

   void clear( void ); // clear to default background color (Black for color displays, off for mono)
   void clear( unsigned char red, unsigned char green, unsigned char blue );

   void refresh( void );

   void render( int x, int y,
                int w, int h,				 // width and height of image
                unsigned short const *pixels,
                int imagexPos=0,			//offset within image to start display
                int imageyPos=0,
                int imageDisplayWidth=0,		//portion of image to display
                int imageDisplayHeight=0
              );
   static void render( int x, int y,
                       int w, int h,				 // width and height of source image
                       unsigned short const *pixels,
                       unsigned short       *dest,
                       unsigned short        destW,
                       unsigned short        destH,
                       int imagexPos=0,			//offset within image to start display
                       int imageyPos=0,
                       int imageDisplayWidth=0,		//portion of image to display
                       int imageDisplayHeight=0
                     );

   void render( unsigned short x, unsigned short y,
                unsigned short w, unsigned short h,
                unsigned short const *pixels,
                unsigned char const  *alpha );
   void render( unsigned short x, unsigned short y,
                unsigned short w, unsigned short h,
                unsigned short const *pixels,
                unsigned short transparentColor,
                unsigned short bgColor );
   static void render( unsigned short x, unsigned short y,
                       unsigned short w, unsigned short h,
                       unsigned short const *pixels,
                       unsigned char const  *alpha,
                       unsigned short       *dest,
                       unsigned short        destW,
                       unsigned short        destH );

   // draw a filled rectangle
   void rect( unsigned short x1, unsigned short y1,
              unsigned short x2, unsigned short y2,
              unsigned char red, unsigned char green, unsigned char blue,
              unsigned char *imageMem = 0,
              unsigned short imageWidth = 0,
              unsigned short imageHeight = 0,
   	      unsigned short imageStride = 0);
   
   // draw a line of specified thickness, points specify top or left of line
   void line( unsigned short x1, unsigned short y1, // only vertical and horizonta supported (for now)
              unsigned short x2, unsigned short y2,
              unsigned char penWidth,
              unsigned char red, unsigned char green, unsigned char blue,
              unsigned char *imageMem = 0,
              unsigned short imageWidth = 0,
              unsigned short imageHeight = 0,
   	      unsigned short imageStride = 0);
   
   // outlined box (4 connecting lines), points specify outside of box
   void box( unsigned short x1, unsigned short y1,
             unsigned short x2, unsigned short y2,
             unsigned char penWidth,
             unsigned char red, unsigned char green, unsigned char blue,
             unsigned char *imageMem = 0,
             unsigned short imageWidth = 0,
             unsigned short imageHeight = 0,
             unsigned short imageStride = 0);


   // render anti-aliased alphamap in specified color to display with clipping
   // always displays top left of source image.
   void antialias( unsigned char const *bmp,
                   unsigned short       bmpWidth,  // row stride
                   unsigned short       bmpHeight, // num rows in bmp
                   unsigned short       xLeft,     // display coordinates: clip to this rectangle
                   unsigned short       yTop,
                   unsigned short       xRight,
                   unsigned short       yBottom,
                   unsigned char red, unsigned char green, unsigned char blue );

   // render anti-aliased alphamap in specified color to 16-bit image with clipping
   void antialias( unsigned char const *bmp,       // input: alphaMap
                   unsigned short       bmpWidth,  // row stride
                   unsigned short       bmpHeight, // num rows in bmp
                   unsigned short       xLeft,     // display coordinates: clip to this rectangle
                   unsigned short       yTop,
                   unsigned short       xRight,
                   unsigned short       yBottom,
                   unsigned char        red, 
                   unsigned char        green, 
                   unsigned char        blue,
                   unsigned char       *imageMem,
                   unsigned short       imageWidth,
                   unsigned short       imageHeight,
   		   unsigned short       imageStride);

   int getFd() const { return fd_ ; }

   // draw a box with specified background color and button highlighting.
   // pressed will highlight bottom-right and shade upper left
#ifdef KERNEL_FB
   void buttonize( bool                 pressed,
                   unsigned char        borderWidth,
                   unsigned short       xLeft,
                   unsigned short       yTop,
                   unsigned short       xRight,
                   unsigned short       yBottom,
                   unsigned char red, unsigned char green, unsigned char blue );
   unsigned short *getRow( unsigned y ){ return (unsigned short *)( (char *)getMem() + ( y * stride) ); }
   void render( bitmap_t const &bmp,
                unsigned        x,
                unsigned        y,
                unsigned        rgb );
#else
   void render( bitmap_t const &bmp,
                unsigned        x,
                unsigned        y );      // implicit 1 = black
#endif 
   void           *getMem( void ) const { return mem_ ; }
   unsigned        getRamOffs( void ) const { return 0 ; }
   unsigned long   pageSize( void ) const { return width_*height_*sizeof(unsigned short); }

   unsigned long   fbOffset( void *pixAddr ) const { return (unsigned long)pixAddr - (unsigned long)mem_ ; }

#ifdef KERNEL_FB_SM501
   unsigned long fb0_offset( void ) const ;
#endif

private:
   fbDevice_t( fbDevice_t const &rhs ); // no copies
   fbDevice_t( char const *name );
   ~fbDevice_t( void );
   int            fd_ ;
   void          *mem_ ;
   unsigned long  memSize_ ;
   unsigned short width_ ;
   unsigned short height_ ;
   unsigned short stride;

   friend fbDevice_t &getFB( char const *devName );
   friend void *flashThread( void *param );
   friend class flashThread_t ;
};

fbDevice_t &getFB( char const *devName = 0 );

#define SCREENWIDTH() getFB().getWidth()
#define SCREENHEIGHT() getFB().getHeight()

unsigned short fbDevice_t :: get16( unsigned long rgb )
{ 
   return get16( (unsigned char)( ( rgb & 0xFF0000 ) >> 16 ),
                 (unsigned char)( ( rgb & 0x00FF00 ) >> 8 ),
                 (unsigned char)rgb );
}

#endif

