#ifndef __FBDEV_H__
#define __FBDEV_H__ "$Id: fbDev.h,v 1.8 2002-12-04 13:56:37 ericn Exp $"

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
 * Revision 1.8  2002-12-04 13:56:37  ericn
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

   void         *getMem( void ) const { return mem_ ; }
   unsigned long getMemSize( void ) const { return memSize_ ; }

   unsigned short *getRow( unsigned y ){ return (unsigned short *)( (char *)mem_ + ( y * ( 2*getWidth() ) ) ); }
   unsigned short &getPixel( unsigned x, unsigned y ){ return getRow( y )[ x ]; }

   static unsigned short get16( unsigned char red, unsigned char green, unsigned char blue );

   //
   // un-mix the screen bits and return 8-bit values for each of red, green, blue
   //
   static unsigned char getRed( unsigned short screenRGB );
   static unsigned char getGreen( unsigned short screenRGB );
   static unsigned char getBlue( unsigned short screenRGB );

   void render( int x, int y,
                int w, int h,				 // width and height of image
                unsigned short const *pixels,
                int imagexPos=0,			//offset within image to start display
                int imageyPos=0,
                int imageDisplayWidth=0,		//portion of image to display
                int imageDisplayHeight=0
              );

   void render( unsigned short x, unsigned short y,
                unsigned short w, unsigned short h,
                unsigned short const *pixels,
                unsigned char const  *alpha );

   // draw a filled rectangle
   void rect( unsigned short x1, unsigned short y1,
              unsigned short x2, unsigned short y2,
              unsigned char red, unsigned char green, unsigned char blue );
   
   // draw a line of specified thickness, points specify top or left of line
   void line( unsigned short x1, unsigned short y1, // only vertical and horizonta supported (for now)
              unsigned short x2, unsigned short y2,
              unsigned char penWidth,
              unsigned char red, unsigned char green, unsigned char blue );
   
   // outlined box (4 connecting lines), points specify outside of box
   void box( unsigned short x1, unsigned short y1,
             unsigned short x2, unsigned short y2,
             unsigned char penWidth,
             unsigned char red, unsigned char green, unsigned char blue );

private:
   fbDevice_t( char const *name );
   ~fbDevice_t( void );
   int            fd_ ;
   void          *mem_ ;
   unsigned long  memSize_ ;
   unsigned short width_ ;
   unsigned short height_ ;
   friend fbDevice_t &getFB( char const *devName );
};

fbDevice_t &getFB( char const *devName = "/dev/fb0" );

#endif

