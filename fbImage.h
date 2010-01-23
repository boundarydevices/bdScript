#ifndef __FBIMAGE_H__
#define __FBIMAGE_H__ "$Id: fbImage.h,v 1.7 2002-12-15 05:41:38 ericn Exp $"

/*
 * fbImage.h
 *
 * This header file declares the fbImage_t class, which 
 * represents and contains a reference to an image in 
 * SM-501 Frame-Buffer memory.
 * 
 * Note that this type of image differs from those in system 
 * RAM in that all frame-buffer images are aligned to 16 bytes
 * (8 pixels) and because alpha is not supported.
 *
 * Change History : 
 *
 * $Log: fbImage.h,v $
 * Revision 1.7  2002-12-15 05:41:38  ericn
 * -Added scaleHorizontal() method
 *
 * Revision 1.6  2006/12/13 21:29:56  ericn
 * -made stride more explicitly bytes vs. pixels
 *
 * Revision 1.4  2006/11/06 10:33:51  ericn
 * -allow construction from fbMem
 *
 * Revision 1.3  2006/10/16 22:35:17  ericn
 * -added validate() method
 *
 * Revision 1.2  2006/08/16 02:38:00  ericn
 * -update to use fbPtr_t, allow 4444 support
 *
 * Revision 1.1  2006/06/14 13:55:22  ericn
 * -Initial import
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbMem.h"
#include "image.h"
#include "sm501alpha.h" // for enumeration

class fbImage_t {
public:
   enum mode_t {
      rgb565    = sm501alpha_t::rgba44,
      rgba4444  = sm501alpha_t::rgba4444
   };

   fbImage_t( void );
   fbImage_t( image_t const &image, 
              mode_t         mode );
   fbImage_t( unsigned x, unsigned y, unsigned w, unsigned h ); // from FB
   fbImage_t( fbPtr_t &mem, unsigned w, unsigned h ); // from fb mem
   fbImage_t( fbImage_t const &rhs ){ mode_ = rhs.mode_ ; w_ = rhs.w_ ; h_ = rhs.h_ ; stride_ = rhs.stride_ ; ptr_ = rhs.ptr_ ; }

   ~fbImage_t( void );

   mode_t   mode( void ) const { return mode_ ; }
   unsigned width( void ) const { return w_ ; }
   unsigned height( void ) const { return h_ ; }
   unsigned stride( void ) const { return stride_ ; }

   unsigned ramOffset( void ) const { return ptr_.getOffs(); }
   void    *pixels( void ) const { return ptr_.getPtr(); }
   unsigned memSize( void ) const { return ptr_.size(); }

   inline bool validate( void ) const { return (0 != ptr_.getPtr()) && ptr_.validate(); }

   fbImage_t *scaleHorizontal( unsigned width ) const ;

private:
   mode_t   mode_ ;
   unsigned w_ ;
   unsigned h_ ;
   unsigned stride_ ;
   fbPtr_t  ptr_ ;
};

#endif

