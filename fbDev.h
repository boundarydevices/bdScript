#ifndef __FBDEV_H__
#define __FBDEV_H__ "$Id: fbDev.h,v 1.15 2004-05-08 14:23:02 ericn Exp $"

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
 * Revision 1.15  2004-05-08 14:23:02  ericn
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


class fbDevice_t {
public:
   bool isOpen( void ) const { return 0 != mem_ ; }

   unsigned short getWidth( void ) const { return width_ ; }
   unsigned short getHeight( void ) const { return height_ ; }

   unsigned long getMemSize( void ) const { return memSize_ ; }

   unsigned short getPixel( unsigned x, unsigned y );
   void           setPixel( unsigned x, unsigned y, unsigned short rgb );

   inline static unsigned short get16( unsigned long rgb );

   static unsigned short get16( unsigned char red, unsigned char green, unsigned char blue );

   //
   // un-mix the screen bits and return 8-bit values for each of red, green, blue
   //
   static unsigned char getRed( unsigned short screenRGB );
   static unsigned char getGreen( unsigned short screenRGB );
   static unsigned char getBlue( unsigned short screenRGB );

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
              unsigned short *imageMem = 0 ,
              unsigned short  imageWidth = 0,
              unsigned short  imageHeight = 0 );
   
   // draw a line of specified thickness, points specify top or left of line
   void line( unsigned short x1, unsigned short y1, // only vertical and horizonta supported (for now)
              unsigned short x2, unsigned short y2,
              unsigned char penWidth,
              unsigned char red, unsigned char green, unsigned char blue,
              unsigned short *imageMem = 0 ,
              unsigned short  imageWidth = 0,
              unsigned short  imageHeight = 0 );
   
   // outlined box (4 connecting lines), points specify outside of box
   void box( unsigned short x1, unsigned short y1,
             unsigned short x2, unsigned short y2,
             unsigned char penWidth,
             unsigned char red, unsigned char green, unsigned char blue,
             unsigned short *imageMem = 0 ,
             unsigned short  imageWidth = 0,
             unsigned short  imageHeight = 0 );


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
                   unsigned short      *imageMem,
                   unsigned short       imageWidth,
                   unsigned short       imageHeight );

   // draw a box with specified background color and button highlighting.
   // pressed will highlight bottom-right and shade upper left
#ifdef CONFIG_BD2003
   void buttonize( bool                 pressed,
                   unsigned char        borderWidth,
                   unsigned short       xLeft,
                   unsigned short       yTop,
                   unsigned short       xRight,
                   unsigned short       yBottom,
                   unsigned char red, unsigned char green, unsigned char blue );
   unsigned short *getRow( unsigned y ){ return (unsigned short *)( (char *)mem_ + ( y * ( 2*getWidth() ) ) ); }
#endif 
private:
   void           *getMem( void ) const { return mem_ ; }
   fbDevice_t( fbDevice_t const &rhs ); // no copies
   fbDevice_t( char const *name );
   ~fbDevice_t( void );
   int            fd_ ;
   void          *mem_ ;
   unsigned long  memSize_ ;
   unsigned short width_ ;
   unsigned short height_ ;
   friend fbDevice_t &getFB( char const *devName );
   friend void *flashThread( void *param );
   friend class flashThread_t ;
};

fbDevice_t &getFB( char const *devName = "/dev/fb0" );


unsigned short fbDevice_t :: get16( unsigned long rgb )
{ 
   return get16( (unsigned char)( ( rgb & 0xFF0000 ) >> 16 ),
                 (unsigned char)( ( rgb & 0x00FF00 ) >> 8 ),
                 (unsigned char)rgb );
}

#endif

