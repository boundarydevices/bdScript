#ifndef __IMAGE_H__
#define __IMAGE_H__ "$Id: image.h,v 1.3 2006-05-14 14:43:44 ericn Exp $"

/*
 * image.h
 *
 * This header file declares the image_t class, which is
 * a really simple structure used to keep track of the
 * book-keeping and allocation of image data. 
 *
 * All of the real work is done by the imgXXX functions 
 * (imgGIF, imgPNG... ).
 *
 *
 *
 * Change History : 
 *
 * $Log: image.h,v $
 * Revision 1.3  2006-05-14 14:43:44  ericn
 * -allow image_t construction
 *
 * Revision 1.2  2005/01/01 16:11:49  ericn
 * -add image types
 *
 * Revision 1.1  2003/10/18 19:16:16  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

class image_t {
public:
   enum type_e {
      unknown = 0,
      imgJPEG,
      imgPNG,
      imgGIF
   };

   image_t( void )
      : pixData_( 0 )
      , width_( 0 )
      , height_( 0 )
      , alpha_( 0 ){}

   image_t( void const *pixData,		// should be new []'d, image will take ownership
	    unsigned width,
	    unsigned height,
	    void const *alpha = 0 )		// 			"
      : pixData_( pixData )
      , width_( width )
      , height_( height )
      , alpha_( alpha ){}

   ~image_t( void );
               
   // use this before re-use
   void unload( void );
   
   // or this if you have control
   inline void disown( void ){ pixData_ = 0 ; }

   void const    *pixData_ ;
   unsigned short width_ ;
   unsigned short height_ ;
   void const    *alpha_ ;

private:
   image_t( image_t const & ); // no copies
};

#endif

