#ifndef __FBDEV_H__
#define __FBDEV_H__ "$Id: fbDev.h,v 1.2 2002-10-31 02:06:23 ericn Exp $"

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
 * Revision 1.2  2002-10-31 02:06:23  ericn
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

