#ifndef __CBMIMAGE_H__
#define __CBMIMAGE_H__ "$Id: cbmImage.h,v 1.1 2003-05-26 22:08:19 ericn Exp $"

/*
 * cbmImage.h
 *
 * This header file declares the cbmImage_t class,
 * which is used to build a set of print commands and
 * data to represent an image on the CBM BD2-1220 
 * receipt printer.
 *
 * Use of this class allows the creation of a single
 * string to represent a section of 'tape' so that 
 * the image can be produced in one swell foop.
 *
 * Usage entails :
 *       constructor  creating the object specifying the 
 *                    desired width and height.
 *                    The default state of the image is
 *                    completely white (no dots)
 *       setPixel     placing black pixels into the image.
 *                    Note that pixel addressing is done in 
 *                    'portrait' mode, with x representing the
 *                    dot on the print head and y representing
 *                    the number of stepper motor steps from
 *                    the top of the image.
 *       getData      Get the image command/data string
 *       getLength    get the length of the command/data 
 *                    string. The data can't be NULL-terminated 
 *                    string because it contains binary pixel
 *                    data.
 *       destructor   cleaning up
 *
 * Change History : 
 *
 * $Log: cbmImage.h,v $
 * Revision 1.1  2003-05-26 22:08:19  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */


class cbmImage_t {
public:
   enum {
      maxWidth_ = 432
   };

   cbmImage_t( unsigned short widthPix,
               unsigned short heightPix );
   ~cbmImage_t( void );

   void setPixel( unsigned x,       // dots from left
                  unsigned y );     // steps from top

   void const *getData( void ) const { return data_ ; }
   unsigned    getLength( void ) const { return length_ ; }

// private:
   unsigned short const specWidth_ ;   // app-specified width
   unsigned short const specHeight_ ;
   unsigned short const actualWidth_ ; // padded to byte boundary
   unsigned short const actualHeight_ ;
   unsigned       const rowsPerSegment_ ;
   unsigned       const rowsLastSegment_ ;
   unsigned       const numSegments_ ;
   unsigned       const bytesPerSegment_ ;
   unsigned       const length_ ;
   unsigned char *const data_ ;
};

#endif

